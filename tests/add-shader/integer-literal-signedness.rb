# Test suite for Graphics.add_shader
# Author: white-axe (2026)

# Creative Commons CC0: To the extent possible under law, white-axe has
# dedicated all copyright and related and neighboring rights to this script
# to the public domain worldwide.
# https://creativecommons.org/publicdomain/zero/1.0/

# Run the suite via the "customScript" field in mkxp.json.

# Decimal integer literals are signed unless they have a 'u' suffix, in which case they're unsigned.
# Hexadecimal and octal integer literals are always unsigned.
# Boolean literals (and Booleans in general) are signed integers where true is 1 and false is 0.
error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <int foo[false - 1 >= 0] = {0};> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 1 passed' : 'Test 1 failed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <int foo[true - 1 >= 0] = {0};> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 2 failed' : 'Test 2 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <int foo[false - 1 >= 0u] = {0};> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 3 failed' : 'Test 3 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <int foo[true - 1 >= 0u] = {0};> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 4 failed' : 'Test 4 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <int foo[false - 1 >= 0U] = {0};> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 5 failed' : 'Test 5 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <int foo[true - 1 >= 0U] = {0};> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 6 failed' : 'Test 6 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <int foo[false - 1 >= 0x0] = {0};> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 7 failed' : 'Test 7 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <int foo[true - 1 >= 0x0] = {0};> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 8 failed' : 'Test 8 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <int foo[false - 1 >= 0x0u] = {0};> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 9 failed' : 'Test 9 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <int foo[true - 1 >= 0x0u] = {0};> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 10 failed' : 'Test 10 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <int foo[false - 1 >= 0x0U] = {0};> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 11 failed' : 'Test 11 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <int foo[true - 1 >= 0x0U] = {0};> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 12 failed' : 'Test 12 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <int foo[false - 1 >= 0X0u] = {0};> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 13 failed' : 'Test 13 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <int foo[true - 1 >= 0X0u] = {0};> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 14 failed' : 'Test 14 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <int foo[false - 1 >= 0X0U] = {0};> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 15 failed' : 'Test 15 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <int foo[true - 1 >= 0X0U] = {0};> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 16 failed' : 'Test 16 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <int foo[false - 1 >= 01 - 01] = {0};> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 17 failed' : 'Test 17 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <int foo[true - 1 >= 01 - 01] = {0};> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 18 failed' : 'Test 18 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <int foo[false - 1 >= 01u - 01u] = {0};> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 19 failed' : 'Test 19 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <int foo[true - 1 >= 01u - 01u] = {0};> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 20 failed' : 'Test 20 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <int foo[false - 1 >= 01U - 01U] = {0};> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 21 failed' : 'Test 21 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <int foo[true - 1 >= 01U - 01U] = {0};> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 22 failed' : 'Test 22 passed')
