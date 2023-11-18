<img src="https://raw.github.com/AdeptLanguage/Adept/master/.github/README_logo.png" width="275" height="240">

## Adept
A blazing fast language for general purpose programming.

[Download Adept v2.7 for Windows](https://github.com/IsaacShelton/Adept/releases)

[Download Adept v2.7 for MacOS](https://github.com/IsaacShelton/Adept/releases)

[Download Adept v2.7 Cross-Compilation Extensions](https://github.com/IsaacShelton/AdeptCrossCompilation/releases/)

## Resources
[Adept v2.7 Documentation](https://github.com/AdeptLanguage/Adept/wiki)

[Adept v2.7 Standard Library](https://github.com/AdeptLanguage/AdeptImport)

[Adept v2.7 Working with Domestic and Foreign Libraries](https://github.com/IsaacShelton/AdeptWorkingWithC)

[Adept v2.7 MacOS Homebrew Tap](https://github.com/AdeptLanguage/homebrew-tap)

[Adept v2.7 Vim Syntax Highlighting](https://github.com/IsaacShelton/AdeptVim)

[Adept v2.7 VS-Code Syntax Highlighting](https://github.com/IsaacShelton/AdeptVSCode)

[Adept v2.7 VS-Code Language Server](https://github.com/IsaacShelton/AdeptVSCodeLanguage)

[Adept v2.7 Sublime Text Syntax Highlighting](https://github.com/AdeptLanguage/AdeptSublimeText)

[Adept v2.7 Geany Syntax Highlighting](https://github.com/iahung3/adept.geany.syntaxcoloring)

[Adept v2.7 JEdit Syntax Highlighting](https://github.com/iahung3/adept.jedit.syntaxcoloring]

## Command-Line Usage

`adept [filename] [options]`

- filename - default is 'main.adept'
- options - secondary compiler options

You can optionally use `adept2-7` instead of `adept` if you have multiple versions installed.

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
    name String = "Alan"
    age int = 36
    height uint = 189
    
    print("Name is " + name)
    print("Age is " + age)
    print("Height is " + height + " cm")
}
```

### Functions

```
import basics

func main {
    greet("Alice")
}

func greet(name String) {
    print("Hello " + name)
}
```

### Records

```
import basics

record Person (name String, age int) {
    func print {
        print(this.name + " is " + this.age + " years old")
    }
}

func main {
    Person("John Smith", 36).print()
}
```

### Structs

```
import basics

struct Configuration (
    filename String,
    numBatteries int,
    numPorts int,
){
    constructor(filename String) {
        this.filename = filename.commit()
        this.numBatteries = 4
        this.numPorts = 10
    }
}

func toString(config Configuration) String {
    return config.filename + ":" + config.numBatteries + ":" + config.numPorts
}

func main {
    print(Configuration("settings.config"))
}
```

### Pointers

```
import basics

func main {
    x, y int = 0

    x++
    y++
    addOneTo(&x)

    print("x = " + x)
    print("y = " + y)
}

func addOneTo(pointerToNumber *$T~__number__) {
    (*pointerToNumber)++
}
```

### Conditionals and Loops

```
import basics

func main {
    name String = scan("What is your name? ")
    age int = scanInt("How old are you? ")
    
    // If conditional
    if name == "Isaac" {
        print("Hello Isaac :)")
    } else {
        print("Nice to meet you " + name)
    }
    
    // Unless conditional
    unless age > 21 {
        print("You are too young to drink")
    }
    
    // While loops
    while age < 18 {
        print("**birthday**")
        age += 1
    }
    
    print("You are now old enough to smoke")
    
    // Until loops
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

record Invitation (to, from String, priority int)

func main {
    invites <Invitation> List
    invites.add(Invitation("Isaac", "Peter", 4))
    invites.add(Invitation("Mr. Smith", "Mrs. Smith", 12))
    invites.add(Invitation("James", "Paul", 3))

    each Invitation in invites {
        printf("invites[%zu] = %S\n", idx, toString(it))
    }
}

func toString(invite Invitation) String {
    return invite.to + " invited " + invite.from + " with priority " + invite.priority
}
```

### Ownership

```
/*
    For values that use ownership-based memory management
    (e.g. String, List, Grid)
    
    we must transfer ownership if we want to keep them
    alive for longer than their owner's scope
*/

import basics

func main {
    everyone <String> List = getEveryoneAttending()
    
    each fullname String in everyone {
        print("=> " + fullname)
    }
}

func getEveryoneAttending() <String> List {
    everyone <String> List

    person1 String = getFullnameReturnImmediately("Alice", "Golden")
    person2 String = getFullnameStoreAndThenLaterReturn("Bob", "Johnson")

    // Commit ownership of strings held by 'person1' and 'person2'
    // to be managed by the list
    everyone.add(person1.commit())
    everyone.add(person2.commit())

    // Commit ownership of the list to the caller
    return everyone.commit()
}

func getFullnameReturnImmediately(firstname, lastname String) String {
    // '.commit()' is not necessary here
    return firstname + " " + lastname
}

func getFullnameStoreAndThenLaterReturn(firstname, lastname String) String {
    fullname String = firstname + " " + lastname
    
    // Ownership of the result is held by 'fullname',
    // so we must transfer ownership to the caller in order
    // to keep it alive after this function returns

    // '.commit()' is necessary here
    return fullname.commit()
}
```

### Loop Labels

```
import basics

func main {
    print(makeNumericSkewer())
    print(makeAlphabetSkewer())
}

func makeNumericSkewer String {
    // Example output: `0-2-4-6-8-10-13-16-19-22-25-28`

    skewer String

    while continue preparing {
        skewer.append(skewer.length)

        if skewer.length < 30 {
            skewer.append("-")
            continue preparing
        }
    }

    return skewer
}

func makeAlphabetSkewer String {
    // Example output: `a-c-e-g-i-k-m-o-q-s-u-w-y`

    skewer String

    while still_making_skewer : skewer.length < 30 {
        skewer.append('a'ub + skewer.length as ubyte)

        if skewer[skewer.length - 1] == 'y'ub {
            break still_making_skewer
        }

        skewer.append("-")
    }

    return skewer
}
```

### Named Expressions and Global Variables

```
import basics

// Will be re-evaluated with each use
define GREETING = "Welcome"
define STRANGER_NAME = "guest"
define WELCOME_MESSAGE = GREETING + " " + STRANGER_NAME + "!"

// Will only be evaluated once when the program starts
ARCH_STRING String = #get __arm64__ ? "arm64" : #get __x86_64__ ? "x86_64" : "other"

func main {
    print(WELCOME_MESSAGE)
    print("You are using architecture: " + ARCH_STRING)
}
```

### Low-Level Dynamic Allocation

```
import 'sys/cstdio.adept'
import 'sys/cstdlib.adept'
import 'sys/cstring.adept'

func main int {
    withMallocAndFree('Will', 'Johnson')
    withNewAndDelete('John', 'Wilson')
    return 0
}

func withMallocAndFree(firstname, lastname *ubyte) void {
    // Manual C-String manipulation using malloc, free, and sprintf
    
    fullname *ubyte = malloc(strlen(firstname) + strlen(lastname) + 2)
    defer free(fullname)

    sprintf(fullname, '%s %s', firstname, lastname)
    printf('Fullname is: %s\n', fullname)
}

func withNewAndDelete(firstname, lastname *ubyte) void {
    // Manual C-String manipulation using new, delete, and sprintf
    
    fullname *ubyte = new ubyte * (strlen(firstname) + strlen(lastname) + 2)
    defer delete fullname

    sprintf(fullname, '%s %s', firstname, lastname)
    printf('Fullname is: %s\n', fullname)
}
```

### Command-Line Arguments
```
import basics

func main(argc int, argv **ubyte) {
    // C-Style
    // Print out each argument specified
    each *ubyte in [argv, argc] {
        printf("args[%zu] = %s\n", idx, argv[idx])
    }

    // Collect arguments into string list
    args <String> List = Array(argv, argc).map(func &stringConstant)

    // Adept-style
    // Print out arguments again, except in cleaner way
    each String in args {
        printf("args[%zu] = %S\n", idx, it)
    }

    if args.contains("-h") || args.contains("--help") {
        print("You asked for help, but I have no help to show you")
    }
}
```

### Function Pointers

```
import basics
import random

func sum(a, b int) int = a + b
func mul(a, b int) int = a * b

func main {
    randomize()

    doCalculation func(int, int) int = null
    
    if normalizedRandom() < 0.5 {
        doCalculation = func &sum
    } else {
        doCalculation = func &mul
    }

    print("Result of 8 and 13 is " + doCalculation(8, 13))
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
import 'sys/cstdio.adept'

func main(argc int, argv **ubyte) int {
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
pragma compiler_version '2.7'
pragma project_name 'my_cool_project'
pragma optimization aggressive

import basics

func main {
    print("Hello World")
}
```

### Primitive Types

```
func main {
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
    
    // x as Type   is equivalent to   cast Type x
    
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
    print(sum(8, 13))
    print(sum(3.14159, 0.57721))
    print(sum(' 'ub, '!'ub))
    print(sum(true, false))
    print(sum("Hello", " World"))
}
```

### Polymorphic Structures

```
import basics

record <$T> Couple (first, second $T)

func main {
    coord <int> Couple
    coord.first = 3
    coord.second = 4
    print("Distance is: " + coord.distance())
    
    socks <String> Couple = Couple("Left Sock", "Right Sock")
    print(socks)
}

func toString(couple <$T> Couple) String {
    return toString(couple.first) + " " + toString(couple.second)
}

func distance(this *<$T~__number__> Couple) $T {
    const x double = cast double this.first
    const y double = cast double this.second
    return sqrt(x * x + y * y) as $T
}
```

### Delayed Method Declaration

```
import basics

record Unit (hp int) {
    func damage(atk int) {
        this.hp -= atk
    }
}

func heal(this *Unit, pts int) {
    this.hp += pts
}

func main {
    unit Unit = Unit(10)
    unit.damage(7)
    unit.heal(4)
    print("Remaining HP: " + unit.hp)
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
        printf("my_integers[%zu] = %d\n", idx, it)
    }
}
```

### Classes and Virtual Dispatch
```
import basics

class Shape () {
    constructor {}
    
    virtual func draw {}
}

class Rectangle extends Shape (w, h float) {
    constructor(w, h float) {
        this.w = w
        this.h = h
    }
    
    override func draw {
        printf("Rectangle %f by %f\n", this.w, this.h)
    }
}

class Circle extends Shape (radius float) {
    constructor(radius float) {
        this.radius = radius
    }
    
    override func draw {
        printf("Circle with radius %f\n", this.radius)
    }
}

func main {
    shapes <*Shape> List
    
    defer {
        each *Shape in shapes, delete it
    }
     
    shapes.add(new Rectangle(4.0, 5.0) as *Shape)
    shapes.add(new Circle(9.0) as *Shape)

    each *Shape in shapes {
        it.draw()
    }
}
```

## Applications in Adept 2.0

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
- [(2.6) Shared Pointer Demo](https://github.com/IsaacShelton/AdeptSharedPtr)
- [(2.7) RTS Parody Game](https://github.com/IsaacShelton/GalaxyCraft)
- [(2.8) Windows GUI Examples](https://github.com/IsaacShelton/AdeptWin32Examples)

## Popular Libraries and Ports
- [(2.5) Box2D](https://github.com/IsaacShelton/Box2D)
- [(2.6) SharedPtr](https://github.com/IsaacShelton/AdeptSharedPtr)

## Additional Syntax Examples
[See examples folder](https://github.com/IsaacShelton/Adept/tree/master/tests/e2e/src)

## Thank you for sponsoring Adept ❤️
- Fernando Dantas
