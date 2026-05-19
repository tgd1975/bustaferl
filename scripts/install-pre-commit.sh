#!/bin/sh
# Install a git pre-commit hook that runs `make ci` (format-check + lint
# + tidy + native + build) before every commit, blocking any commit that
# would land with a lint or tidy regression.
#
# Hook runtime is ~30-45 s on a warm cache (clang-tidy dominates).
# Remove with: rm .git/hooks/pre-commit
#
# See docs/main-refactor-plan.md §11.3.

set -e
cd "$(git rev-parse --show-toplevel)"
cat > .git/hooks/pre-commit <<'HOOK'
#!/bin/sh
exec make ci
HOOK
chmod +x .git/hooks/pre-commit
echo "pre-commit hook installed; remove with: rm .git/hooks/pre-commit"
