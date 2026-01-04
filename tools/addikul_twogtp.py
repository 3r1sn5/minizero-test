#!/usr/bin/env python
import argparse
import datetime
import socket
import subprocess
import sys
from typing import List, Tuple


def read_gtp_response(proc: subprocess.Popen) -> str:
    lines: List[str] = []
    response_started = False
    while True:
        line = proc.stdout.readline()
        if line == "":
            break
        line = line.rstrip("\n")
        if not response_started:
            if line == "":
                continue
            if line.startswith("=") or line.startswith("?"):
                response_started = True
                lines.append(line)
            continue
        if line == "":
            break
        lines.append(line)
    return "\n".join(lines)


def send_gtp_command(proc: subprocess.Popen, command: str) -> str:
    proc.stdin.write(command + "\n")
    proc.stdin.flush()
    return read_gtp_response(proc)


def parse_gtp_ok(response: str) -> str:
    if not response:
        raise RuntimeError("empty response")
    if response[0] == '?':
        raise RuntimeError(response)
    if response[0] != '=':
        raise RuntimeError(response)
    return response[1:].strip()


def start_engine(command: str) -> subprocess.Popen:
    return subprocess.Popen(
        command,
        shell=True,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
    )


def initialize_engine(proc: subprocess.Popen, board_size: int) -> None:
    parse_gtp_ok(send_gtp_command(proc, "protocol_version"))
    parse_gtp_ok(send_gtp_command(proc, "name"))
    parse_gtp_ok(send_gtp_command(proc, "clear_board"))
    parse_gtp_ok(send_gtp_command(proc, f"boardsize {board_size}"))


def run_game(black_cmd: str, white_cmd: str, board_size: int, max_moves: int) -> Tuple[float, int]:
    black = start_engine(black_cmd)
    white = start_engine(white_cmd)
    move_count = 0
    try:
        initialize_engine(black, board_size)
        initialize_engine(white, board_size)
        color = "black"
        result_override = None
        while move_count < max_moves:
            current = black if color == "black" else white
            opponent = white if color == "black" else black
            response = parse_gtp_ok(send_gtp_command(current, f"genmove {color}"))
            move = response.strip()
            if move.lower() == "resign":
                result_override = -1.0 if color == "black" else 1.0
                break
            if move.lower() == "pass":
                break
            parse_gtp_ok(send_gtp_command(opponent, f"play {color} {move}"))
            color = "white" if color == "black" else "black"
            move_count += 1
        if result_override is not None:
            return result_override, move_count
        score = parse_gtp_ok(send_gtp_command(black, "final_score"))
        return float(score), move_count
    finally:
        black.terminate()
        white.terminate()


def write_dat(output_path: str,
              black_cmd: str,
              white_cmd: str,
              results: List[Tuple[float, int, int, str]],
              board_size: int) -> None:
    timestamp = datetime.datetime.now().strftime("%B %d, %Y at %I:%M:%S %p %Z").strip()
    host = socket.gethostname()
    with open(output_path, "w", encoding="utf-8") as f:
        f.write("# Black: minizero\n")
        f.write(f"# BlackCommand: {black_cmd}\n")
        f.write("# BlackLabel: minizero:1.0\n")
        f.write("# BlackVersion: 1.0\n")
        f.write(f"# Date: {timestamp}\n")
        f.write(f"# Host: {host}\n")
        f.write("# Komi: 0\n")
        f.write("# Referee: -\n")
        f.write(f"# Size: {board_size}\n")
        f.write("# White: minizero\n")
        f.write(f"# WhiteCommand: {white_cmd}\n")
        f.write("# WhiteLabel: minizero:1.0\n")
        f.write("# WhiteVersion: 1.0\n")
        f.write("# Xml: 0\n")
        f.write("#\n")
        f.write("#GAME\tRES_B\tRES_W\tRES_R\tALT\tDUP\tLEN\tTIME_B\tTIME_W\tCPU_B\tCPU_W\tERR\tERR_MSG\n")
        for idx, (result, move_count, alt, err_msg) in enumerate(results):
            res_b = f"{result:.6f}"
            res_w = f"{-result:.6f}"
            err = "1" if err_msg else "0"
            fields = [
                str(idx),
                res_b,
                res_w,
                "?",
                str(alt),
                "-",
                str(move_count),
                "0",
                "0",
                "0",
                "0",
                err,
                err_msg,
            ]
            f.write("\t".join(fields) + "\n")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--black", required=True)
    parser.add_argument("--white", required=True)
    parser.add_argument("--games", type=int, required=True)
    parser.add_argument("--boardsize", type=int, required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--max-moves", type=int, default=500)
    args = parser.parse_args()

    results: List[Tuple[float, int, int, str]] = []
    for game_idx in range(args.games):
        alt = game_idx % 2
        black_cmd = args.black if alt == 0 else args.white
        white_cmd = args.white if alt == 0 else args.black
        try:
            result, move_count = run_game(black_cmd, white_cmd, args.boardsize, args.max_moves)
            results.append((result, move_count, alt, ""))
        except Exception as exc:
            results.append((0.0, 0, alt, str(exc)))

    write_dat(args.out, args.black, args.white, results, args.boardsize)
    return 0


if __name__ == "__main__":
    sys.exit(main())