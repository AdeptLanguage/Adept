@echo off
pushd "%~dp0"

call :compile address
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile aliases
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile andor
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile array_access
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile assignment
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile break
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile break_to
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile cast
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile character_literals
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile conditionless_break_label
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile constants
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile continue
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile continue_to
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile defer
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile deprecated
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile dereference
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile fixed_array
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile funcptr
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile functions
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile globals
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile hello_world
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile hexadecimal
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile if
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile ifelse
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile import
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile mathassign
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile member
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile methods
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile multiple_declaration
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile native_linking
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile new_delete
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile new_dynamic
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile not
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile null
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile order
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile package
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile package_use
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile pragma
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile primitives
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile repeat_args
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile repeat_fields
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile return_ten
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile sizeof
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile stdcall
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile structs
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile terminate_join
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile truefalse
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile undef
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile unless
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile unlesselse
if %errorlevel% neq 0 popd & exit /b %errorlevel%
REM This file should always fail
call :compile unsupported
if %errorlevel% neq 1 popd & exit /b %errorlevel%
call :compile until
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile until_break
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile varargs
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile variables
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile while
if %errorlevel% neq 0 popd & exit /b %errorlevel%
call :compile while_continue
if %errorlevel% neq 0 popd & exit /b %errorlevel%

:: Delete debugging dump files if present
if exist ast.txt   del /F ast.txt
if exist infer.txt del /F infer.txt
if exist ir.txt    del /F ir.txt

echo All tests compiled successfully!
popd
exit /b 0

:compile
echo ^=^> Compiling Test Program^: ^'%~1^'
adept_debug %~1/main.adept %~2
exit /b %errorlevel%
GOTO:EOF
