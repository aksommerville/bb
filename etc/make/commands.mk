ifneq (,$(strip $(EXE_DEMO)))
  demo:$(EXE_DEMO) $(DEMO_DATA_OUTPUTS);$(EXE_DEMO)
  demo-%:$(EXE_DEMO) $(DEMO_DATA_OUTPUTS);$(EXE_DEMO) $*
else
  demo demo-%:;echo "EXE_DEMO unset" ; exit 1
endif

ifneq (,$(strip $(EXE_CLI)))
  run:$(EXE_CLI);$(EXE_CLI)
  run-%:$(EXE_CLI);$(EXE_CLI) $*
else
  run run-%:;echo "EXE_CLI unset" ; exit 1
endif

clean:;rm -rf mid out
