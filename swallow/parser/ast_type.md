# The following are the various ast types generated by the parser. The checkbox beside them tells if the codegen is working for it

- [ ] variable - As the name suggests. Subtypes - int_variable, void_variable , float_variable, str_variable, bool_variable, list_variable, dictionary_variable
- [ ] constant - Variables whose value you can not change. Subtypes - int_constant, void_constant , float_variable, str_variable, bool_constant, list_constant, dictionary_constant
- [ ] try - Used to run a piece of code and check if it is working or else will perform the task in the except block.
- [ ] except- Performs the code if try block fails.
- [ ] Finally- executes the code after try and except
- [ ] if, else, elif- Various conditional statement
- [ ] and,or,not,in,is - As the name suggests.
- [ ] raw_string- like this `r"hi\n"` will print `hi\n`
- [ ] formated_string- `f"{a} and {b}"` will print `a_value and b_value`
- [ ] Ccode- If you call c functions
- [ ] asign- Refers to the equals sign(=)
- [ ] binary_operator - Refers to the following `['+','-','*','/','^','//','%']`
- [ ] compare - Refers to the following `['==', '!=', '<=', '>=', '>','<']`
- [ ] bool,string,void,pointer,dictionary,list- Various types of valus a variable can hold
- [ ] pass-Do nothing
- [ ] return- Returns value
- [ ] break- Stop the execution of a loop
- [ ] continue- Continus the execution of a loop
- [ ] dot,comma,bracket- As the name suggests
- [ ] function_call- Calls a function
- [ ] function_define- Used to define a function
- [ ] newline - as the name suggests `\n`