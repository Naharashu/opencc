static inline void f1() {}
static inline void f2() {
	f1();
}

void foo() { f1(); }

void main() { foo(); };
