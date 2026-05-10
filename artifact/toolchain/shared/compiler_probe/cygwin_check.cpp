// Cygwin compiler detection probe.
// Compile with the compiler under test; run and check exit code.
// Exit 0 → Cygwin-targeted compiler (__CYGWIN__ defined at compile time).
// Exit 1 → not Cygwin.
//
// Used by is_cygwin_compiler() in compiler_probe.py.
// Kept as a separate file so it's easy to inspect and reason about.
int main() {
#ifdef __CYGWIN__
    return 0;
#else
    return 1;
#endif
}
