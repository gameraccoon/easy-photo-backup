#!/bin/bash

# if we already run in a new terminal window
if [ "$1" = "--internal" ]; then
    sas_code="$2"
    out_file="$3"
    exit_file="$4"

    # catch closing the terminal window
    trap '[[ -s "$exit_file" ]] || echo 1 > "$exit_file"' EXIT HUP INT TERM

    echo "Pairing code: $sas_code" >&2

    read -p "Did the client accept the code? (Y/N): " confirm

    case "$confirm" in
        [Yy])
            echo -e "Give this client a name:\n> \c"
            read client_name
            
            echo "$client_name" > "$out_file"
            echo "Added the client" >&2

            echo 0 > "$exit_file" # signal success
            sleep 1
            exit 0
            ;;
    esac

    echo "Cancel pairing" >&2
    echo 1 > "$exit_file" # signal failure
    sleep 1
    exit 0
fi

# else if we run normally

sas_code="$1"

SCRIPT_PATH=$(readlink -f "$0")

out_file=$(mktemp)
exit_file=$(mktemp)

TERM_CMD=""
for t in x-terminal-emulator gnome-terminal konsole xfce4-terminal alacritty kitty xterm; do
    if command -v "$t" >/dev/null 2>&1; then
        TERM_CMD="$t"
        break
    fi
done

if [ -z "$TERM_CMD" ]; then
    echo "Error: No terminal emulator found." >&2
    rm -f "$out_file" "$exit_file"
    exit 1
fi

LAUNCH_ARGS=( "$SCRIPT_PATH" "--internal" "$sas_code" "$out_file" "$exit_file" )

# some special handling for running terminal emulators
case "$TERM_CMD" in
    gnome-terminal)
        "$TERM_CMD" -- bash -c 'exec "$@"' _ "${LAUNCH_ARGS[@]}"
        ;;
    xfce4-terminal)
        "$TERM_CMD" -x bash -c 'exec "$@"' _ "${LAUNCH_ARGS[@]}"
        ;;
    *)
    	# all others
        "$TERM_CMD" -e bash -c 'exec "$@"' _ "${LAUNCH_ARGS[@]}"
        ;;
esac

# wait until the script finishes
while [ ! -s "$exit_file" ]; do
    sleep 0.1
done

EXIT_CODE=$(cat "$exit_file")

if [ "$EXIT_CODE" -eq 0 ]; then
    cat "$out_file"
fi

rm -f "$out_file" "$exit_file"

exit "$EXIT_CODE"
