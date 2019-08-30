
# Adept
A blazing fast language for general purpose programming.

## Command-Line Usage
`adept [filename] [options]`

- filename - default is 'main.adept'
- options - secondary compiler options

You can optionally use `adept2` instead of `adept` if you have multiple versions installed.

## Basic Functionality
### Variables
```
import 'sys/cstdio.adept'

func main(in argc int, in argv **ubyte) int {
    my_name *ubyte = 'Isaac'
    printf('My name is %s\n', my_name)
    return 0
}
```

### Functions
```
import 'sys/cstdio.adept'

func main(in argc int, in argv **ubyte) int {
    greet()
    return 0
}

func greet() void {
    printf('Hello There!\n')
}
```

### Structures
```
import 'sys/cstdio.adept'

struct Person (name *ubyte, age int)

func main(in argc int, in argv **ubyte) int {
    john_smith Person; john_smith.create('John Smith', 36)
    john_smith.print()
    return 0
}

func create(this *Person, name *ubyte, age int) void {
    this.name = name
    this.age = age
}

func print(this *Person) void {
    printf('%s is %d years old\n', this.name, this.age)
}
```

### Dynamic Memory Allocation (using malloc/free)
```
import 'sys/cstdio.adept'
import 'sys/cstdlib.adept'
import 'sys/cstring.adept'

func main(in argc int, in argv **ubyte) int {
    firstname *ubyte = 'Will'
    lastname *ubyte = 'Johnson'

    fullname *ubyte = malloc(strlen(firstname) + strlen(lastname) + 2)
    defer free(fullname)

    sprintf(fullname, '%s %s', firstname, lastname)
    printf('Fullname is: %s\n', fullname)
    return 0
}
```

### Dynamic Memory Allocation (using new/delete)
```
import 'sys/cstdio.adept'
import 'sys/cstring.adept'

func main(in argc int, in argv **ubyte) int {
    firstname *ubyte = 'Will'
    lastname *ubyte = 'Johnson'

    fullname *ubyte = new ubyte * (strlen(firstname) + strlen(lastname) + 2)
    defer delete fullname

    sprintf(fullname, '%s %s', firstname, lastname)
    printf('Fullname is: %s\n', fullname)
    return 0
}
```

### Conditionals
```
import 'sys/cstdio.adept'

func main(in argc int, in argv **ubyte) int {
    if argc == 1 {
        printf('Please pass in some arguments\n')
        return 1
    }

    i int = 0; while i != argc {
        printf('Argument %d is "%s"\n', i, argv[i])
        i += 1
    }
    return 0
}
```

### Negated Conditionals
```
import 'sys/cstdio.adept'

func main(in argc int, in argv **ubyte) int {
    unless argc != 1 {
        printf('Please pass in some arguments\n')
        return 1
    }

    i int = 0; until i == argc {
        printf('Argument %d is "%s"\n', i, argv[i])
        i += 1
    }
    return 0
}
```

### Loop Labels
```
import 'sys/cstdio.adept'

func main(in argc int, in argv **ubyte) int {
    countdown int = 0
    prepare(&countdown); launch(&countdown)
    return 0
}

func prepare(inout countdown *int) void {
    while continue preparing {
        *countdown += 1
        if *countdown != 10, continue preparing
    }
}

func launch(inout countdown *int) void {
    bad_launch bool = false

    until ready_to_launch : *countdown == 0 {
        *countdown -= 1
        printf('Counting Down... %d\n', *countdown)

        if bad_launch, break ready_to_launch
    }

    unless bad_launch, printf('Lift Off!\n')
}
```

### Constant Values
```
import 'sys/cstdio.adept'

PI == 3.14159265
TAU == PI * 2

func main(in argc int, in argv **ubyte) int {
    printf('PI is %f\n', PI)
    printf('TAU is %f\n', TAU)
    return 0
}
```

### Function Pointers
```
import 'sys/cstdio.adept'

func main(in argc int, in argv **ubyte) int {
    calculate func(int, int) int = func &sum
    printf('calculate(13, 8) == %d\n', calculate(13, 8))
    return 0
}

func sum(in a int, in b int) int {
    return a + b
}
```

### Multiple Declaration
```
import 'sys/cstdio.adept'

func main(in argc int, in argv **ubyte) int {
    a, b int = 13
    return a + b
}
```

### Defer Statements
```
import 'sys/cstdio.adept'

func main(in argc int, in argv **ubyte) int {
    defer printf('I will be printed last\n')
    defer printf('I will be printed second\n')
    defer printf('I will be printed first\n')
    printf('I will be printed before anyone else\n')
    return 0
}
```

### Undef Keyword
```
import 'sys/cstdio.adept'

func main(in argc int, in argv **ubyte) int {
    // Will be initialized to 0
    zero_value int

    // Will be initialized to null
    null_pointer *ulong

    // Will be undefined (left uninitialized)
    undefined_value int = undef
    undefined_pointer *ulong = undef

    printf('%d == 0, %d == ?\n', zero_value, undefined_value)
    printf('%p == null, %p == ?\n', null_pointer, undefined_pointer)
    return 0
}
```

### Pragma Directives
```
pragma compiler_version '2.0'
pragma project_name 'pragma_directives_example'
pragma optimization aggressive

import 'sys/cstdio.adept'

func main(in argc int, in argv **ubyte) int {
    printf('Hello World\n')
    return 0
}
```

### Primitive Types
```
import 'sys/cstdio.adept'

func main(in argc int, in argv **ubyte) int {
    // 8-bit Types
    a_byte   byte   = 0sb
    a_ubyte  ubyte  = 0ub

    // 16-bit Types
    a_short  short  = 0ss
    a_ushort ushort = 0us

    // 32-bit Types
    an_int   int    = 0si
    a_uint   uint   = 0ui
    a_float  float  = 0.0f

    // 64-bit Types
    a_long   long   = 0sl
    a_ulong  ulong  = 0ul
    a_double double = 0.0d
    a_usize  usize  = 0uz

    // 64-bit or 32-bit depending on the system
    a_ptr    ptr    = null
    int_ptr  *int   = null

    return 0
}
```

### Type Casting
```
import 'sys/cstdio.adept'

func main(in argc int, in argv **ubyte) int {
    x int = 8
    y int = 13

    // Primitive value casting
    positionX double = cast double x
    positionY double = cast double y

    // Arbitrary pointer casting
    ptr_cast_result *uint = cast *uint &x

    // Expression result casting
    sum usize = cast usize (positionX + positionY)

    return 0
}
```

# Applications in Adept 2.0
- [Tic-Tac-Toe (2.0)](https://github.com/IsaacShelton/AdeptTicTacToe)
- [Neural Network (2.0)](https://github.com/IsaacShelton/AdeptNeuralNetwork)
- [2D Platformer (2.0)](https://github.com/IsaacShelton/Adept2DPlatformer)
- [Minesweeper (2.1)](https://github.com/IsaacShelton/AdeptMinesweeper)
- [Another 2D Platformer (2.2)](https://github.com/IsaacShelton/Tangent)

# Syntax Examples
[See examples folder](https://github.com/IsaacShelton/Adept/tree/master/tests)

# Differences from Adept 1.1
- Updated and improved syntax
- Simplified programming structure
- Type Inference
- Improved error reporting
- Faster compilation speeds
- Newer LLVM Backend
- Written in C instead of C++
- Better system to build the compiler
