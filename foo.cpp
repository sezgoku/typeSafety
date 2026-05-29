struct foo{
	int bar;
	int* bar_pointer;
}
struct bar{
	int value;
	foo struct_value;
	int add(){
		return 'a' + 1;
	}
}
