.PHONY: win run cafe nomap clean default_map

# Regenerate src/default_map.inc from assets/map01.map.
# Run this by hand whenever you've edited the map and want the embedded
# fallback to match. Not invoked automatically by any build target.
default_map:
	@if [ ! -f assets/map01.map ]; then \
		echo "ERROR: assets/map01.map not found — cannot regenerate default_map.inc"; \
		exit 1; \
	fi
	@awk '{ gsub(/"/, "\\\""); print "    \"" $$0 "\\n\"" }' assets/map01.map > src/default_map.inc
	@echo "regenerated src/default_map.inc from assets/map01.map"

win:
	@test -f src/default_map.inc || (echo "ERROR: src/default_map.inc missing — run 'make default_map' first"; exit 1)
	@$(MAKE) -C src/win -f Make_Win

run:
	@test -f src/default_map.inc || (echo "ERROR: src/default_map.inc missing — run 'make default_map' first"; exit 1)
	@$(MAKE) -C src/win -f Make_Win
	@./windows/tower.exe

cafe:
	@test -f src/default_map.inc || (echo "ERROR: src/default_map.inc missing — run 'make default_map' first"; exit 1)
	@$(MAKE) -C src/cafe -f Make_Cafe

nomap:
	@test -f src/default_map.inc || (echo "ERROR: src/default_map.inc missing — run 'make default_map' first"; exit 1)
	@$(MAKE) -C src/cafe -f Make_Cafe NOMAP=1

clean:
	@$(MAKE) -C src/win -f Make_Win clean
	@$(MAKE) -C src/cafe -f Make_Cafe clean