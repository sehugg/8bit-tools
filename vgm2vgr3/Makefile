CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -std=c99 -DVGR3_FEAT_EXT=1

all: vgm2vgr3

# VGR3 encoder (see new.md, vgr3_format.h). Self-verifies with vgr3_play.c.
vgm2vgr3: vgm2vgr3.c vgr3_play.c vgr3_format.h vgr3_play.h vgm_format.h
	$(CC) $(CFLAGS) -o $@ vgm2vgr3.c vgr3_play.c

SAMPLES_DIR ?= samples
VGR3_DIR ?= vgr3
ROUNDTRIP_OUT := .roundtrip

# Encode every sample to $(VGR3_DIR)/<name>.vgr3. vgm2vgr3 is
# self-verifying, so a file is only written when its decode matches the
# source frame by frame (and --loop replays two full passes).
samples: vgm2vgr3
	@mkdir -p $(VGR3_DIR)
	@pass=0; fail=0; total=0; \
	for f in $$(find $(SAMPLES_DIR) -type f -iname '*.vgm' | sort); do \
		base=$$(basename "$$f"); \
		out="$(VGR3_DIR)/$${base%.*}.vgr3"; \
		log="$(VGR3_DIR)/$${base%.*}.log"; \
		if ./vgm2vgr3 --loop "$$f" "$$out" > "$$log" 2>&1; then \
			rm -f "$$log"; \
			pass=$$((pass+1)); sz=$$(wc -c < "$$out"); total=$$((total+sz)); \
			echo "OK    $$out  ($$sz bytes)"; \
		else \
			fail=$$((fail+1)); echo "FAIL  $$base  (see $$log)"; \
		fi; \
	done; \
	echo "---"; \
	echo "samples: $$pass built, $$fail failed, $$total bytes"; \
	[ $$fail -eq 0 ]

# Encodes every sample to VGR3; vgm2vgr3 exits non-zero (and writes
# nothing) unless the decoded file matches the source frame by frame.
roundtrip: vgm2vgr3
	@mkdir -p $(ROUNDTRIP_OUT)
	@pass=0; fail=0; total=0; \
	for f in $$(find $(SAMPLES_DIR) -type f \( -iname '*.vgm' \) | sort); do \
		base=$$(basename "$$f"); \
		vgr="$(ROUNDTRIP_OUT)/$$base.vgr"; \
		log="$(ROUNDTRIP_OUT)/$$base.log"; \
		if ./vgm2vgr3 "$$f" "$$vgr" > "$$log" 2>&1; then \
			pass=$$((pass+1)); sz=$$(wc -c < "$$vgr"); total=$$((total+sz)); \
			echo "PASS  $$base  ($$sz bytes)"; \
		else \
			fail=$$((fail+1)); echo "FAIL  $$base  (see $$log)"; \
		fi; \
	done; \
	echo "---"; \
	echo "roundtrip: $$pass passed, $$fail failed, $$total bytes"; \
	[ $$fail -eq 0 ]

# Same as roundtrip but with --loop, so sources with no loop point get
# a synthetic JUMP back to frame 0. The encoder's self-check already
# replays two full passes whenever loopFrame is set, so a clean run here
# verifies the loop-around state (including the frame-0 reset) too.
roundtrip-loop: vgm2vgr3
	@mkdir -p $(ROUNDTRIP_OUT)
	@pass=0; fail=0; \
	for f in $$(find $(SAMPLES_DIR) -type f \( -iname '*.vgm' \) | sort); do \
		base=$$(basename "$$f"); \
		vgr="$(ROUNDTRIP_OUT)/loop-$$base.vgr"; \
		log="$(ROUNDTRIP_OUT)/loop-$$base.log"; \
		if ./vgm2vgr3 --loop "$$f" "$$vgr" > "$$log" 2>&1; then \
			pass=$$((pass+1)); echo "PASS  $$base"; \
		else \
			fail=$$((fail+1)); echo "FAIL  $$base  (see $$log)"; \
		fi; \
	done; \
	echo "---"; \
	echo "roundtrip-loop: $$pass passed, $$fail failed"; \
	[ $$fail -eq 0 ]

clean:
	rm -f vgm2vgr3 *.o
	rm -rf $(ROUNDTRIP_OUT)

.PHONY: all clean samples roundtrip roundtrip-loop
