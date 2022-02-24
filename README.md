# Adept

A blazing fast language for general purpose programming.

[Download Adept v2.6 for Windows](https://github.com/IsaacShelton/Adept/releases)

[Download Adept v2.6 for MacOS](https://github.com/IsaacShelton/Adept/releases)

[Download Adept v2.6 Cross-Compilation Extensions](https://github.com/IsaacShelton/AdeptCrossCompilation/releases/)

## Resources
[Adept v2.6 Documentation](https://github.com/AdeptLanguage/Adept/wiki)

[Adept v2.6 Vim Syntax Highlighting](https://github.com/IsaacShelton/AdeptVim)

[Adept v2.6 VS-Code Syntax Highlighting](https://github.com/IsaacShelton/AdeptVSCode)

[Adept v2.6 VS-Code Basic Syntax Checking](https://github.com/IsaacShelton/AdeptVSCodeLanguage)

[Adept v2.6 Standard Library](https://github.com/AdeptLanguage/AdeptImport)

[Adept v2.6 Working with Domestic and Foreign Libraries](https://github.com/IsaacShelton/AdeptWorkingWithC)

[Adept v2.6 MacOS Homebrew Tap](https://github.com/AdeptLanguage/homebrew-tap)

## Command-Line Usage

`adept [filename] [options]`

- filename - default is 'main.adept'
- options - secondary compiler options

You can optionally use `adept2-6` instead of `adept` if you have multiple versions installed.

## Basic Functionality

### Hello World

```
import basics

func main {
    print("Hello World!")
}
```

### Variables

```
import basics

func main {
    name String = "Isaac"
    print("My name is " + name)
}
```

### Functions

```
import basics

func main {
    greet("Isaac")
}

func greet(name String) {
    print("Welcome " + name)
}
```

### Structures

```
import basics

struct Person (name String, age int)

func main {
    john_smith Person = person("John Smith", 36)
    john_smith.print()
}

func person(name String, age int) Person {
    person POD Person
    person.name = name.commit()
    person.age = age
    return person
}

func print(this *Person) {
    print("% is % years old" % this.name % this.age)
}
```

### Pointers

```
import basics

func main {
    width, height int
    getWidthAndHeight(&width, &height)
    print("Width = %, Height = %" % width % height)
}

func getWidthAndHeight(out width, height *int) {
    *width = 640
    *height = 480
}
```

### Conditionals

```
import basics

func main {
    name String = scan("What is your name? ")
    age int = skimInt("How old are you? ")
    
    if name == "Isaac" {
        print("Hello Isaac :)")
    } else {
        print("Nice to meet you " + name)
    }
    
    unless age > 21 {
        print("You are too young to drink")
    }
    
    while age < 18 {
        print("**birthday**")
        age += 1
    }
    
    print("You are now old enough to smoke")
    
    until age >= 21 {
        print("**birthday**")
        age += 1
    }
    
    print("You are now old enough to drink")
}
```

### Lists

```
import basics

func main {
    invites <Invitation> List
    invites.add(invitation("Isaac", "Peter", 4))
    invites.add(invitation("Mr. Smith", "Mrs. Smith", 12))
    invites.add(invitation("James", "Paul", 3))
}

struct Invitation (to, from String, priority int)

func invitation(to, from String, priority int) Invitation {
    invitation POD Invitation
    invitation.to = to.commit()
    invitation.from = from.commit()
    invitation.priority = priority
    return invitation
}
```

### Ownership

```
import basics

list_of_everyone <String> List

func main {
    populateListOfEveryone()
    
    each String in list_of_everyone {
        print("=> " + it)
    }
}

func populateListOfEveryone {
    fullname1 String = getFullnameVersion1("Isaac", "Shelton")
    fullname2 String = getFullnameVersion2("Isaac", "Shelton")
    
    list_of_everyone.add(fullname1.commit())
    list_of_everyone.add(fullname2.commit())
}

func getFullnameVersion1(firstname, lastname String) String {
    return firstname + " " + lastname
}

func getFullnameVersion2(firstname, lastname String) String {
    fullname String = firstname + " " + lastname
    // ... other statements ...
    return fullname.commit()
}
```

### Loop Labels

```
import basics

func main {
    countdown int = 0
    prepare(&countdown)
    launch(&countdown)
}

func prepare(inout countdown *int) {
    while continue preparing {
        *countdown += 1
        if *countdown != 10, continue preparing
    }
}

func launch(inout countdown *int) {
    bad_launch bool = false

    until ready_to_launch : *countdown == 0 {
        *countdown -= 1
        print("Counting Down... %" % *countdown)
        if bad_launch, break ready_to_launch
    }

    unless bad_launch, print("Lift Off!")
}
```

### Constant Values

```
import basics

PI == 3.14159265
TAU == PI * 2

func main {
    print("PI is %" % PI)
    print("TAU is %" % TAU)
}
```

### Dynamic Memory Allocation

```
import cstdio
import cstdlib
import cstring

func main(in argc int, in argv **ubyte) int {
    usingMallocAndFree('Will', 'Johnson')
    usingNewAndDelete('John', 'Wilson')
    return 0
}

func usingMallocAndFree(firstname, lastname *ubyte) void {
    fullname *ubyte = malloc(strlen(firstname) + strlen(lastname) + 2)
    defer free(fullname)

    sprintf(fullname, '%s %s', firstname, lastname)
    printf('Fullname is: %s\n', fullname)
}

func usingNewAndDelete(firstname, lastname *ubyte) void {
    fullname *ubyte = new ubyte * (strlen(firstname) + strlen(lastname) + 2)
    defer delete fullname

    sprintf(fullname, '%s %s', firstname, lastname)
    printf('Fullname is: %s\n', fullname)
}
```

### Function Pointers

```
import basics

func sum(a, b int) int = a + b

func main {
    calculate func(int, int) int = func &sum
    print("calculate(8, 13) = %" % calculate(8, 13))
}
```

### Defer Statements

```
import basics

func main {
    defer print("I will be printed last")
    defer print("I will be printed second")
    defer print("I will be printed first")
    print("I will be printed before anyone else")
}
```

### Undef Keyword

```
import cstdio

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
pragma compiler_version '2.5'
pragma project_name 'pragma_directives_example'
pragma optimization aggressive

import basics

func main {
    print("Hello World")
}
```

### Primitive Types

```
import cstdio

func main(in argc int, in argv **ubyte) int {
    // 8-bit Types
    a_bool   bool   = false
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
import basics

func main {
    value int = 1234
    
    // Primitive value casting
    result1 double = value as double
    result2 double = cast double value
    
    // Arbitrary pointer casting
    result3 *uint = &value as *uint
    result4 *uint = cast *uint &value
    
    // Expression result casting
    result5 usize = (value + 1) as usize
    result6 usize = cast usize (value + 1)
}
```

### Runtime Type Information

```
import basics

func main {
    print("Every type used in this program: ")
    
    each *AnyType in [__types__, __types_length__] {
        print(" => " + stringConstant(it.name))
    }
    
    print("...")
    print("Each member of type 'String':")
    
    string_type *AnyStructType = typeinfo String as *AnyStructType
    repeat string_type.length {
        field_name String = stringConstant(string_type.member_names[idx])
        field_type String = stringConstant(string_type.members[idx].name)
        print(" => " + field_name + " " + field_type)
    }
}
```

### Conditional Compilation

```
#default should_fake_windows   false
#default should_fake_macos     false
#default enable_secret_feature false

#if enable_secret_feature
    #print "Doing super secret feature stuff..."
    #set should_fake_windows true
    #set should_fake_macos   true
#end

import basics

func main {
    #if should_fake_windows && should_fake_macos
        print("Hello from Windows and MacOS???")
    #elif __windows__ || should_fake_windows
        print("Hello on Windows!")
    #elif __macos__ || should_fake_macos
        print("Hello on MacOS!")
    #end
}
```

### Polymorphism

```
import basics

func sum(a, b $T) $T = a + b

func main {
    print("%" % sum(8, 13))
    print("%" % sum(3.14159, 0.57721))
    print("%" % sum(' 'ub, '!'ub))
    print("%" % sum(true, false))
    print("%" % sum("Hello", "World"))
}
```

### Polymorphic Structures

```
import basics

struct <$T> Couple (first, second $T)

func main {
    socks <String> Couple
    socks.first = "Left Sock"
    socks.second = "Right Sock"
}
```

### Delayed Method Declaration

```
import basics

struct Unit (health int) {
    func damage(amount int) {
        this.health -= amount
    }
}

func heal(this *Unit, amount int) {
    this.health += amount
}

func main {
    unit Unit
    unit.health = 10
    unit.damage(7)
    unit.heal(4)
}
```

### Intrinsic Loop Variables

```
import basics

func main {
    my_integers <int> List
    
    // Add numbers 0..9 inclusive to list
    repeat 10, my_integers.add(idx)
    
    // Square each number in the list
    each int in my_integers, it = it * it
    
    // Print each number
    each int in my_integers {
        print("my_integers[%] = %" % idx % it)
    }
}
```

### Spontaneous Variable Declarations

```
import basics
import random

func main {
    //   def variable_name Type   -  Zero-Initialized Spontaneous Variable
    // undef variable_name Type   -  Undefined Spontaneous Variable
    
    color int = 0x112233FF
    getColorComponents(color, undef r int, undef g int, undef b int)
    print("Color Components: % % %" % r % g % b)
    
    randomize()
    maybeGetAlphaChannel(color, def a int)
    print("Alpha Channel: %" % a)
}

func getColorComponents(color int, out r, g, b *int) void {
    *r = (color & 0xFF000000) >> 24
    *g = (color & 0x00FF0000) >> 16
    *b = (color & 0x0000FF00) >> 8
}

func maybeGetAlphaChannel(color int, out a *int) void {
    if random() < 0.5, *a = (color & 0x000000FF)
}
```

# Applications in Adept 2.0

- [(2.0) Tic-Tac-Toe](https://github.com/IsaacShelton/AdeptTicTacToe)
- [(2.0) Neural Network](https://github.com/IsaacShelton/AdeptNeuralNetwork)
- [(2.0) 2D Platformer](https://github.com/IsaacShelton/Adept2DPlatformer)
- [(2.1) HexGL](https://github.com/IsaacShelton/HexGL)
- [(2.1) Minesweeper](https://github.com/IsaacShelton/AdeptMinesweeper)
- [(2.2) Another 2D Platformer](https://github.com/IsaacShelton/Tangent)
- [(2.2) A* Path Finding](https://github.com/IsaacShelton/AdeptPathFinding)
- [(2.3) Creature Gathering Game](https://github.com/IsaacShelton/Tadpole)
- [(2.4) Card Game](https://github.com/IsaacShelton/GenericCardGame)
- [(2.5) MiniBox Multiplayer Gamepad](https://github.com/IsaacShelton/MiniBox)

# Popular Library Ports
- [(2.5) Box2D](https://github.com/IsaacShelton/Box2D)

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



#### Thank you for sponsoring Adept: ❤️

- Fernando Dantas
