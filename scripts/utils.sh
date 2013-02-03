ifgetips="scripts/ifgetips"

function machine_info() {
        echo "DATE: $(date +%Y%m%d.%H%M%S)"
        echo "MACHINE INFO"
        uname -a
	getconf GNU_LIBC_VERSION
	getconf GNU_LIBPTHREAD_VERSION
        LC_ALL=C $ifgetips
	cat /proc/cpuinfo
	~/bin/coresinfo
}

function git_info() {
	git log -n 1
	git status
	git diff
}

function gethname() {
        h=$(LC_ALL=C $ifgetips | egrep -oe '[^[:space:]]+\.in\.barrelfish\.org' | sed -e 's/\.in\.barrelfish\.org//')
        if [ -z "$h" ]; then
                h=$(hostname)
        fi
        echo $h
}
