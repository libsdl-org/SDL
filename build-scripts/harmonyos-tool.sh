#!/usr/bin/bash

HVIGORWPATH=`which hvigorw 2>/dev/null`
if [ "x$HVIGORWPATH" == "x" ]; then
    echo "Couldn't find hvigorw!"
    echo "Please make sure the HarmonyOS command line tools are in your PATH." 1>&2
    exit 1
fi

HDCPATH=`which hdc 2>/dev/null`
if [ "x$HDCPATH" == "x" ]; then
    echo "Couldn't find hdc!"
    echo "Please make sure the HarmonyOS command line tools are in your PATH." 1>&2
    exit 1
fi

CLTBINPATH=`dirname "$HVIGORWPATH"`
CLTPATH=`dirname "$CLTBINPATH"`
CLT=`realpath "$CLTPATH"`
#echo "CLT is '$CLT'"

need_usage=0

APPJSON=""
PROJDIR="$1"
if [ "x$PROJDIR" == "x" ]; then
    need_usage=1
else
    PROJDIR=`realpath "$PROJDIR"`
    APPJSON="$PROJDIR/AppScope/app.json5"
    if [ ! -f "$APPJSON" ]; then
        echo "Can't find '$APPJSON'. Is project directory correct?" 1>&2
        echo "" 1>&2
        need_usage=1
    fi
fi

do_clean=0
do_build=0
do_uninstall=0
do_install=0
do_kill=0
do_launch=0
do_log=0
do_debug=0
do_anything=0

shift  # dump projectdir
while [ $# -gt 0 ]; do
    do_anything=1
    case $1 in
        --clean)
            do_clean=1
            shift
            ;;
        --build)
            do_build=1
            shift
            ;;
        --install)
            do_install=1
            shift
            ;;
        --uninstall)
            do_uninstall=1
            shift
            ;;
        --kill)
            do_kill=1
            shift
            ;;
        --launch)
            do_launch=1
            shift
            ;;
        --log)
            do_log=1
            shift
            ;;
        --debug)
            do_launch=1
            do_debug=1
            shift
            ;;
        *)
            echo "Unknown option '$1'" 1>&2
            need_usage=1
            shift
            ;;
    esac
done

if [ "$do_anything" == "0" ]; then
    need_usage=1
fi

if [ "$need_usage" == "1" ]; then
    echo "USAGE: $0 <project_dir> [--kill] [--uninstall] [--build] [--install] [--launch] [--debug] [--log]" 1>&2
    echo "" 1>&2
    exit 1
fi

if [ "$do_build" == "1" ]; then
    if [ "$do_debug" == "1" ]; then
        do_install=1
    elif [ "$do_launch" == "1" ]; then
        do_install=1
    fi
fi

# Here we go!

BUNDLE=`grep -F bundleName "$PROJDIR/AppScope/app.json5" |perl -w -p -e 's/\A\s*\"bundleName\"\s*\:\s*\"(.*?)\".*?\Z/$1/i;'`
if [ "x$BUNDLE" == "x" ]; then
    echo "Failed to determine bundle name! Aborting!" 1>&2
    exit 1
fi

echo "BUNDLE is '$BUNDLE'"

#set -x

cd "$PROJDIR"

if [ "$do_kill" == "1" ]; then
    echo "KILLING..."
    hdc shell aa force-stop $BUNDLE || exit 1
fi

if [ "$do_uninstall" == "1" ]; then
    echo "UNINSTALLING..."
    hdc uninstall $BUNDLE || exit 1
fi

if [ "$do_clean" == "1" ]; then
    echo "CLEANING..."
    hvigorw clean || exit 1
fi

if [ "$do_build" == "1" ]; then
    echo "BUILDING..."
    hvigorw assembleHap || exit 1
fi

if [ "$do_debug" == "1" ]; then
    hdc shell mkdir -p data/local/tmp/debugserver/$BUNDLE
    hdc file send "$CLT/sdk/default/hms/native/lldb/aarch64-linux-ohos/lldb-server" data/local/tmp/debugserver/$BUNDLE
    hdc shell chmod 755 data/local/tmp/debugserver/$BUNDLE/lldb-server
fi

if [ "$do_install" == "1" ]; then
    echo "INSTALLING..."
    hdc install "$PROJDIR/entry/build/default/outputs/default/entry-default-signed.hap" || exit 1
fi

# Catch sigint, kill the backgrounded `hdc hilog` then exit.
trap_sigint() {
    if [ "$HILOGPID" != "" ]; then
        kill -9 "$HILOGPID"
    fi
    wait "$HILOGPID" 2>/dev/null
    echo ""
    #echo "Killed $HILOGPID"
    exit 0
}

HILOGPID=""
if [ "$do_log" == "1" ]; then
    trap trap_sigint INT

    # This wild bash syntax puts `hdc` in the background, and pipes its output through `grep`, but `$!` will have hdc's process ID.
    # The ridiculous perl regular expression basically filters out a ton of garbage from each line of the log. You can remove it entirely and just grep for the bundle name, though!
    hdc hilog > >(grep -F --line-buffered "/$BUNDLE/" |perl -w -p -e 's/\A\d+\-\d+\s+(\d+\:\d+\:\d+)\.\d+\s+\d+\s+\d+\s+(.)\s+.*?\/.*?\/(.*?)\:\s+/$1 $2 $3: /;') & HILOGPID="$!"
fi

if [ "$do_launch" == "1" ]; then
    echo "LAUNCHING..."
    # !!! FIXME: maybe this makes the process wait on launch until debugger is attached?
    #hdc shell aa appdebug -b $BUNDLE
    hdc shell aa start -a EntryAbility -b $BUNDLE || exit 1
fi

if [ "$do_debug" == "1" ]; then
    echo "DEBUGGING..."
    hdc shell aa attach -b $BUNDLE
    hdc shell aa process -a EntryAbility -b $BUNDLE -D "/data/local/tmp/debugserver/$BUNDLE/lldb-server platform --listen unix-abstract:///lldb-server/platform.sock"

    PID=`hdc shell ps -w -o PID,ARGS |grep -F "$BUNDLE" |head -n 1 |awk '{print $1}'`
    if [ "x$PID" == "x" ]; then
        echo "Failed to determine PID! Aborting!" 1>&2
        exit 1
    fi

    echo "PID is $PID"

    trap - INT
    "$CLT/sdk/default/openharmony/native/llvm/bin/lldb" \
        -O "platform select remote-ohos" \
        -O "platform connect unix-abstract-connect:///lldb-server/platform.sock" \
        -O "settings append target.exec-search-paths \"$PROJDIR/entry/build/default/intermediates/cmake/default/obj/arm64-v8a\"" \
        -O "attach $PID"
    trap trap_sigint INT
fi

if [ "$HILOGPID" != "" ]; then
    #echo "$0: WAITING ON $HILOGPID"
    wait "$HILOGPID"
fi

