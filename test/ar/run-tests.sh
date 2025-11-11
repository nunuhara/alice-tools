#!/usr/bin/env bash

cd $(dirname "$0")

function test_alicepack {
    local FILES=("src/テスト1.x" "src/テスト2.x" "src/sub/テスト3.x")

    if ! ${ALICE:-alice} ar pack alicepack.manifest; then
        echo alicepack: pack failed
        return 1
    fi

    rm -rf out
    if ! ${ALICE:-alice} ar extract -o out alicepack.afa; then
        echo alicepack: extract failed
        return 1
    fi

    for f in ${FILES[@]}; do
        if [ -f "out/$f" ]; then
            if ! diff "out/$f" "$f"; then
                echo alicepack: contents of file $f differ
                return 1
            fi
        else
            echo alicepack: file $f missing in output
            return 1
        fi
    done
    echo "#ALICEPACK" test passed
    return 0
}

function test_batchpack {
    local FILES=("テスト1.ex" "テスト2.ex" "sub/テスト3.ex")

    if [ ! -e dst ]; then
        mkdir dst
    fi

    if ! ${ALICE:-alice} ar pack batchpack.manifest; then
        echo batchpack: pack failed
        return 1
    fi

    rm -rf out
    if ! ${ALICE:-alice} ar extract --raw -o out batchpack.afa; then
        echo batchpack: extract failed
        return 1
    fi

    for f in ${FILES[@]}; do
        if [ -f "out/$f" ]; then
            if ! diff "out/$f" "dst/$f"; then
                echo batchpack: contents of file $f differ
                return 1
            fi
        else
            echo batchpack: file $f missing in output
            return 1
        fi
    done
    echo "#BATCHPACK" test passed
    return 0
}

FAILED=0
NTESTS=2

test_alicepack || FAILED=$((FAILED+1))
test_batchpack || FAILED=$((FAILED+1))

echo Passed: $((NTESTS - FAILED))/$NTESTS
echo Failed: $FAILED/$NTESTS

rm -rf dst
rm -rf out
rm -f alicepack.afa
rm -f batchpack.afa

if (( FAILED > 0 )); then
    exit 1
fi
