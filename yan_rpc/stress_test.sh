#!/usr/bin/env bash
# RPC 压测脚本：启动 N 个客户端进程并发调用 DefaultService.Echo，汇总吞吐和延迟
#
#   ./stress_test.sh                  10 个客户端，每个 100 个请求
#   ./stress_test.sh -c 50 -r 200     50 个客户端，每个 200 个请求
#   ./stress_test.sh -c 20 -t 120     20 个客户端，持续 120 秒
#
# 依赖 ZooKeeper 已在 127.0.0.1:2181 运行；服务端未启动时脚本会自动拉起并在结束后关闭
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$ROOT_DIR/build"
CLIENT_CONFIG="$ROOT_DIR/config/rpc_client.json"
SERVER_CONFIG="$ROOT_DIR/config/rpc_server.json"
SERVER_PORT=8989

CLIENTS=10
REQUESTS=100
DURATION=0
SKIP_BUILD=0

usage() {
    cat <<EOF
用法: $0 [-c 客户端数] [-r 每客户端请求数] [-t 持续秒数] [-n]
  -c N   并发客户端数（默认 10）
  -r N   每个客户端发送的请求数（默认 100）
  -t N   按时间压测，持续 N 秒（指定后忽略 -r）
  -n     跳过编译
EOF
    exit 1
}

while getopts "c:r:t:nh" opt; do
    case "$opt" in
        c) CLIENTS="$OPTARG" ;;
        r) REQUESTS="$OPTARG" ;;
        t) DURATION="$OPTARG" ;;
        n) SKIP_BUILD=1 ;;
        *) usage ;;
    esac
done

for v in "$CLIENTS" "$REQUESTS" "$DURATION"; do
    [[ "$v" =~ ^[0-9]+$ ]] || { echo "参数必须是非负整数: $v" >&2; exit 1; }
done
(( CLIENTS > 0 )) || { echo "-c 必须大于 0" >&2; exit 1; }
if (( DURATION > 0 )); then
    REQUESTS=0
else
    (( REQUESTS > 0 )) || { echo "-r 必须大于 0" >&2; exit 1; }
fi

if (( SKIP_BUILD == 0 )); then
    echo "==> 编译"
    cmake -S "$ROOT_DIR" -B "$BUILD_DIR" >/dev/null
    cmake --build "$BUILD_DIR" -j"$(nproc)" --target rpc_server stress_client >/dev/null
fi

# 客户端和服务端的日志都写到 cwd 的 ../log，统一在 build 目录下运行
cd "$BUILD_DIR"

WORK_DIR="$(mktemp -d /tmp/rpc_stress.XXXXXX)"
SERVER_PID=""

cleanup() {
    if [[ -n "$SERVER_PID" ]] && kill -0 "$SERVER_PID" 2>/dev/null; then
        kill -INT "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
    rm -rf "$WORK_DIR"
}
trap cleanup EXIT

port_listening() {
    ss -ltn "sport = :$SERVER_PORT" 2>/dev/null | grep -q LISTEN
}

if port_listening; then
    echo "==> 复用已在 $SERVER_PORT 端口运行的服务端"
else
    echo "==> 启动服务端"
    ./rpc_server "$SERVER_CONFIG" >"$WORK_DIR/server.out" 2>&1 &
    SERVER_PID=$!
    # 服务端先 listen，之后才向 ZooKeeper 注册（中间有 2s 等待），必须等注册完成客户端才能发现它
    for _ in $(seq 1 100); do
        grep -q "REGISTER SUCCESS" "$WORK_DIR/server.out" 2>/dev/null && break
        kill -0 "$SERVER_PID" 2>/dev/null || { cat "$WORK_DIR/server.out"; echo "服务端启动失败" >&2; exit 1; }
        sleep 0.1
    done
    grep -q "REGISTER SUCCESS" "$WORK_DIR/server.out" || { echo "服务端 10 秒内未完成 ZooKeeper 注册" >&2; exit 1; }
fi

if (( DURATION > 0 )); then
    echo "==> 压测: $CLIENTS 个客户端，持续 ${DURATION}s"
else
    echo "==> 压测: $CLIENTS 个客户端 x $REQUESTS 请求 = $((CLIENTS * REQUESTS)) 个请求"
fi

START_NS=$(date +%s%N)
PIDS=()
for i in $(seq 1 "$CLIENTS"); do
    ./stress_client "$CLIENT_CONFIG" "$i" "$REQUESTS" "$DURATION" "$WORK_DIR/result_$i.txt" \
        >"$WORK_DIR/client_$i.out" 2>&1 &
    PIDS+=($!)
done

CLIENT_FAILED=0
for pid in "${PIDS[@]}"; do
    wait "$pid" || CLIENT_FAILED=$((CLIENT_FAILED + 1))
done
END_NS=$(date +%s%N)

if [[ -n "$SERVER_PID" ]] && ! kill -0 "$SERVER_PID" 2>/dev/null; then
    SERVER_EXIT=0
    wait "$SERVER_PID" 2>/dev/null || SERVER_EXIT=$?
    echo "警告: 服务端在压测过程中退出，退出码 $SERVER_EXIT，最后 10 行输出:" >&2
    grep -v "ZOO_" "$WORK_DIR/server.out" | tail -10 >&2
    SERVER_PID=""
fi

python3 - "$WORK_DIR" "$CLIENTS" "$START_NS" "$END_NS" "$CLIENT_FAILED" <<'PY'
import glob, sys

work_dir, clients, start_ns, end_ns, client_failed = sys.argv[1:]
clients, client_failed = int(clients), int(client_failed)
wall = (int(end_ns) - int(start_ns)) / 1e9

success = failed = reconnects = 0
missing = 0
lat = []
for i in range(1, clients + 1):
    try:
        with open(f"{work_dir}/result_{i}.txt") as f:
            head = f.readline().split()
            success += int(head[1]); failed += int(head[2]); reconnects += int(head[3])
            lat.extend(int(line) for line in f if line.strip())
    except (OSError, IndexError, ValueError):
        missing += 1

total = success + failed
lat.sort()
def pct(p):
    return lat[min(len(lat) - 1, int(len(lat) * p / 100))] / 1000 if lat else 0.0

print()
print("================ 压测结果 ================")
print(f"客户端数        : {clients}（异常退出 {client_failed}，无结果 {missing}）")
print(f"总请求数        : {total}")
print(f"成功 / 失败     : {success} / {failed}  成功率 {success / total * 100 if total else 0:.2f}%")
print(f"重连次数        : {reconnects}")
print(f"总耗时          : {wall:.2f} s")
print(f"吞吐 (QPS)      : {success / wall:.1f}")
if lat:
    print(f"延迟 avg        : {sum(lat) / len(lat) / 1000:.3f} ms")
    print(f"延迟 min / max  : {lat[0] / 1000:.3f} / {lat[-1] / 1000:.3f} ms")
    print(f"延迟 P50/P90/P99: {pct(50):.3f} / {pct(90):.3f} / {pct(99):.3f} ms")
print("==========================================")
PY

# 有失败时把首个出错客户端的输出打出来便于排查
if (( CLIENT_FAILED > 0 )); then
    for f in "$WORK_DIR"/client_*.out; do
        if grep -qiE "error|failed" "$f"; then
            echo "---- $(basename "$f") 错误输出（前 20 行）----"
            grep -iE "error|failed" "$f" | head -20
            break
        fi
    done
    exit 1
fi
