# TODOs:
# -- Make it MAC compatible
# -- why auto creation of ensenscount in the top folder

CMAKE_OPTIONS=-G "Unix Makefiles"
PUBLIC_ROOT ?= /tmp
PUBLIC_STAGE := $(PUBLIC_ROOT)/ensenscount_new
PUBLIC_REPO := $(PUBLIC_ROOT)/ensenscount
PUBLIC_REPO_URL ?= git@github.com:ajsnaik/dummy.git
PUBLIC_BRANCH ?= main
PUBLIC_COMMIT_MESSAGE ?= Update public snapshot
PUBLIC_PATHS := \
	.gitignore \
	CMakeLists.txt \
	Makefile \
	README.md \
	baseline \
	include \
	scripts \
	src \
	tests \
	models 

all: release count_tests

debug: lib buildd
	cd buildd; cmake $(CMAKE_OPTIONS) -DCMAKE_BUILD_TYPE=Debug ..
	+make -C buildd/

release: lib buildr
	cd buildr; cmake $(CMAKE_OPTIONS) -DCMAKE_BUILD_TYPE=Release ..
	+make -C buildr/

count_tests: release
	./scripts/tests/run_count_regression.sh

runtests: release
	./ensenscount -f ./data/benchmarks/diabetes/depth_3/0010.json -p -2 -s 2 -k 1

run_naive : release
	./ensenscount -f ./data/benchmarks/diabetes/depth_3/0010.json -p -2 -s 2 -k 1 -M naive
debug_naive : release
	./ensenscount -f model_2_2.json -p -2 -s 2 -k 1 -M naive -D
run_pepin : release
	./ensenscount -f ./data/benchmarks/diabetes/depth_3/0010.json -p -2 -s 2 -k 1 -M pepin --pepin-eps 0.05 --pepin-delta 0.05 -R 42

debug_pepin : release
	./ensenscount -f model_2_2.json -p -2 -s 2 -k 1 -M pepin --pepin-eps 0.1 --pepin-delta 0.1 -R 42 -D

run_sanity_check : release
	./ensenscount -f ./data/benchmarks/diabetes/depth_3/0010.json -p -2 -s 2 -k 1 -X
# ----------------------------
# Create needed foldercheduler---------------------------
lib: lib.tar
	@if [ ! -d lib/cudd-3.0.0 ]; then \
		echo "Extracting lib.tar ..."; \
		tar -xf lib.tar; \
	fi

lib.tar:
	@if [ ! -f lib.tar ]; then \
		echo "Downloading lib.tar ..."; \
		curl -L -o lib.tar https://github.com/vardigroup/ADDMC/raw/master/lib.tar; \
	fi
buildd:
	mkdir -p buildd

buildr:
	mkdir -p buildr

clean:
	rm -rf buildd buildr ensenscount ensenscount_b
	find . -type f -name '*~' -delete

deepclean: clean
	rm -rf lib.tar
	rm -rf lib
	rm -rf debug_output
	rm -rf logs

public:
	@case "$(PUBLIC_STAGE)" in /tmp/*) ;; *) echo "Refusing to update PUBLIC_STAGE outside /tmp: $(PUBLIC_STAGE)"; exit 1; esac
	@case "$(PUBLIC_REPO)" in /tmp/*) ;; *) echo "Refusing to update PUBLIC_REPO outside /tmp: $(PUBLIC_REPO)"; exit 1; esac
	rm -rf "$(PUBLIC_STAGE)"
	mkdir -p "$(PUBLIC_STAGE)"
	rsync -a --relative $(PUBLIC_PATHS) "$(PUBLIC_STAGE)/"

push_public:
	@if [ ! -d "$(PUBLIC_REPO)/.git" ]; then \
		rm -rf "$(PUBLIC_REPO)"; \
		git clone "$(PUBLIC_REPO_URL)" "$(PUBLIC_REPO)"; \
	fi
	@git -C "$(PUBLIC_REPO)" checkout "$(PUBLIC_BRANCH)" >/dev/null 2>&1 || \
		git -C "$(PUBLIC_REPO)" checkout -b "$(PUBLIC_BRANCH)" >/dev/null
	@git -C "$(PUBLIC_REPO)" pull --ff-only origin "$(PUBLIC_BRANCH)" || \
		echo "Remote branch $(PUBLIC_BRANCH) not pulled; continuing with local checkout."
	@if diff -qr --exclude=.git "$(PUBLIC_STAGE)" "$(PUBLIC_REPO)" >/dev/null 2>&1; then \
		echo "No public snapshot changes found."; \
	else \
		find "$(PUBLIC_REPO)" -mindepth 1 -maxdepth 1 ! -name .git -exec rm -rf {} +; \
		rsync -a "$(PUBLIC_STAGE)/" "$(PUBLIC_REPO)/"; \
		git -C "$(PUBLIC_REPO)" add -A -f; \
		git -C "$(PUBLIC_REPO)" commit -m "$(PUBLIC_COMMIT_MESSAGE)"; \
		git -C "$(PUBLIC_REPO)" push origin "$(PUBLIC_BRANCH)"; \
		echo "Updated and pushed $(PUBLIC_REPO) to $(PUBLIC_REPO_URL) ($(PUBLIC_BRANCH))."; \
	fi

runvmtest:
	multipass launch 24.04 -n vmtest -m 4G
	multipass stop vmtest
	multipass exec -n vmtest -- bash -c "sudo apt install -y make cmake g++ curl"
	# multipass exec -n vmtest -- bash -c "sudo snap install multipass-sshfs"
	multipass mount $(pwd) vmtest
	multipass exec -n vmtest -- bash -c "cd ${PWD##*/}; make"
	multipass delete vmtest
	multipass purge

.PHONY: debug release all clean runvmtest count_tests public
