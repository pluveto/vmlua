## lb::vmlua

一个简单的 32 位 Lua 虚拟机，（仅支持了 Lua 的一个子集）

### 组成部分

+ Lexer 词法分析器
+ Parser 语法分析器
+ Emitter 汇编代码生成
+ VM 虚拟机，运行汇编代码
+ Driver 编译驱动，调度上述过程

### 特性

+ 不依赖第三方库
+ 自制交互式单步调试器

## 使用方法

```shell
./scripts/build.sh
```

```shell
./build/vmlua test/what_if.lua
```

### 输入输出样例

```lua
function sum(from, to)
    if from == to then
         return from;
    else
         local prev = sum(from + 1, to);
         return from + prev;
    end
end

function what_if(n)
   if n > 0 then
      return sum(n, n+100);
   else
      if n == 0 then
         return -1;
      else
         return -2;
      end
   end   
end

print(what_if(1));
print(what_if(0));
print(what_if(-1));
```

输出：

```
5050 
-1 
-2
```

Sytax Tree

```
func_decl (what_if ( (n ) ) ( (if_stmt (cond ( binary_op ( > ( id (n) ) ( number (0) ) ) ) (then ( (ret_stmt ( func_call ( sum ( id (n) id (n) number (100)  ) ) )) ) ) (else (if_stmt (cond ( binary_op ( == ( id (n) ) ( number (0) ) ) ) (then ( (ret_stmt ( number (-1) )) ) ) (else (ret_stmt ( number (-2) )) ) )) ) ) ) ) )
```

Tokens output

```
   0 | function 
   1 | sum 
   2 | ( 
   3 | from 
   4 | , 
   5 | to 
   6 | ) 
   7 | if 
   8 | from 
   9 | == 
  10 | to 
  11 | then 
  12 | return 
  13 | from 
  14 | ; 
  15 | else 
  16 | local 
  17 | prev 
  18 | = 
  19 | sum 
  20 | ( 
  21 | from 
  22 | + 
  23 | 1 
  24 | , 
  25 | to 
  26 | ) 
  27 | ; 
  28 | return 
  29 | from 
  30 | + 
  31 | prev 
  32 | ; 
  33 | end 
  34 | end 
  35 | function 
  36 | what_if 
  37 | ( 
  38 | n 
  39 | ) 
  40 | if 
  41 | n 
  42 | > 
  43 | 0 
  44 | then 
  45 | return 
  46 | sum 
  47 | ( 
  48 | n 
  49 | , 
  50 | n 
  51 | 100 
  52 | ) 
  53 | ; 
  54 | else 
  55 | if 
  56 | n 
  57 | == 
  58 | 0 
  59 | then 
  60 | return 
  61 | -1 
  62 | ; 
  63 | else 
  64 | return 
  65 | -2 
  66 | ; 
  67 | end 
  68 | end 
  69 | end 
  70 | print 
  71 | ( 
  72 | what_if 
  73 | ( 
  74 | 1 
  75 | ) 
  76 | ) 
  77 | ; 
  78 | print 
  79 | ( 
  80 | what_if 
  81 | ( 
  82 | 0 
  83 | ) 
  84 | ) 
  85 | ; 
  86 | print 
  87 | ( 
  88 | what_if 
  89 | ( 
  90 | -1 
  91 | ) 
  92 | ) 
  93 | ; 
```

Asm output

```
--------+------------------------------
 OFFSET | INSTRUCTION
--------+------------------------------
       0|     JMP function_done_0 (offset=20)
       1| sum: 
        |     ST FP - 5 -> FP + 0
       2|     ST FP - 4 -> FP + 1
       3|     PUSH FP + 0
       4|     PUSH FP + 1
       5|     COND EQ
       6|     JZ label_else_3 (offset=10)
       7|     PUSH FP + 0
       8|     RETVAL
       8| 
       9|     JMP label_out_3 (offset=20)
      10| label_else_3: 
        |     PUSH FP + 0
      11|     PUSH 1
      12|     ADD
      13|     PUSH FP + 1
      14|     CALL sum(1), nargs=2, nlocals=3
      15|     POP FP + 2
      16|     PUSH FP + 0
      17|     PUSH FP + 2
      18|     ADD
      19|     RETVAL
      19| 
      20| function_done_0: 
        | label_out_3: 
        |     JMP function_done_20 (offset=41)
      21| what_if: 
        |     ST FP - 4 -> FP + 0
      22|     PUSH FP + 0
      23|     PUSH 0
      24|     COND GT
      25|     JZ label_else_22 (offset=32)
      26|     PUSH FP + 0
      27|     PUSH FP + 0
      28|     PUSH 100
      29|     CALL sum(1), nargs=2, nlocals=3
      30|     RETVAL
      30| 
      31|     JMP label_out_22 (offset=41)
      32| label_else_22: 
        |     PUSH FP + 0
      33|     PUSH 0
      34|     COND EQ
      35|     JZ label_else_32 (offset=39)
      36|     PUSH -1
      37|     RETVAL
      37| 
      38|     JMP label_out_32 (offset=41)
      39| label_else_32: 
        |     PUSH -2
      40|     RETVAL
      40| 
      41| function_done_20: 
        | label_out_22: 
        | label_out_32: 
        |     PUSH 1
      42|     CALL what_if(21), nargs=1, nlocals=1
      43|     CALL print@internal, ARGC=1
      44|     PUSH 0
      45|     CALL what_if(21), nargs=1, nlocals=1
      46|     CALL print@internal, ARGC=1
      47|     PUSH -1
      48|     CALL what_if(21), nargs=1, nlocals=1
      49|     CALL print@internal, ARGC=1
```
## 单步调试

目前支持显示栈、指令指针和汇编代码。

启用方法：导入环境变量

```shell
export VM_LUA_DEBUG=1
```

使用方法：

1. 回车或输入 step 进行单步调试。输入 `mem <addr>` 查看内存。
2. 输入 `mem <reg> <offset>` 查看寄存器对应内存。
3. 在汇编预览图中，`*` 就是指令指针的位置。

![image](https://user-images.githubusercontent.com/50045289/179490366-a2bcf7a0-3755-4427-b5bd-dd8d414a4f74.png)
