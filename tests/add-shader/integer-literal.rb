# Test suite for Graphics.add_shader
# Author: white-axe (2026)

# Creative Commons CC0: To the extent possible under law, white-axe has
# dedicated all copyright and related and neighboring rights to this script
# to the public domain worldwide.
# https://creativecommons.org/publicdomain/zero/1.0/

# Run the suite via the "customScript" field in mkxp.json.

# Decimal integer literals without a suffix must either be less than 2147483648 (2 to the power of 31) or be greater than 18446744071562067968 (2 to the power of 64 minus 2 to the power of 31) and less than 18446744073709551616 (2 to the power of 64).
# Decimal integer literals with a suffix must be less than 18446744073709551616 (2 to the power of 64).
# Otherwise there will be a syntax error.
error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <uint foo = 2147483648;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 1 passed' : 'Test 1 failed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <uint foo = 2147483647;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 2 failed' : 'Test 2 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <uint foo = 18446744071562067967;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 3 passed' : 'Test 3 failed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <uint foo = 18446744071562067968;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 4 failed' : 'Test 4 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <uint foo = 18446744073709551616;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 5 passed' : 'Test 5 failed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <uint foo = 18446744073709551615;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 6 failed' : 'Test 6 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <uint foo = 18446744071562067967u;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 7 failed' : 'Test 7 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <uint foo = 18446744071562067968u;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 8 failed' : 'Test 8 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <uint foo = 18446744073709551616u;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 9 passed' : 'Test 9 failed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <uint foo = 18446744073709551615u;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 10 failed' : 'Test 10 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <uint foo = 18446744073709551615U;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 11 failed' : 'Test 11 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <uint foo = 18446744073709551616l;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 12 passed' : 'Test 12 failed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <uint foo = 18446744073709551615l;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 13 failed' : 'Test 13 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <uint foo = 18446744073709551615L;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 14 failed' : 'Test 14 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <uint foo = 18446744073709551616ul;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 15 passed' : 'Test 15 failed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <uint foo = 18446744073709551615ul;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 16 failed' : 'Test 16 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <uint foo = 18446744073709551615ul;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 17 failed' : 'Test 17 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <uint foo = 18446744073709551615lu;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 18 failed' : 'Test 18 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <uint foo = 18446744073709551615UL;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 19 failed' : 'Test 19 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <uint foo = 0xffffffffffffffffuL;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 20 failed' : 'Test 20 passed')

# Hexadecimal and octal integer literals without a suffix must be less than 4294967296 (2 to the power of 32).
# Hexadecimal and octal integer literals with a suffix must be less than 18446744073709551616 (2 to the power of 64).
# Otherwise there will be a syntax error.
error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <uint foo = 0x100000000;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 21 passed' : 'Test 21 failed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <uint foo = 0xffffffff;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 22 failed' : 'Test 22 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <uint foo = 0x10000000000000000u;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 23 passed' : 'Test 23 failed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <uint foo = 0xffffffffffffffffu;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 24 failed' : 'Test 24 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <uint foo = 0xffffffffffffffffU;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 25 failed' : 'Test 25 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <uint foo = 0x10000000000000000l;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 26 passed' : 'Test 26 failed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <uint foo = 0xffffffffffffffffl;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 27 failed' : 'Test 27 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <uint foo = 0xffffffffffffffffL;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 28 failed' : 'Test 28 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <uint foo = 0x10000000000000000ul;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 29 passed' : 'Test 29 failed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <uint foo = 0xfffffffffffffffful;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 30 failed' : 'Test 30 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <uint foo = 0xfffffffffffffffful;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 31 failed' : 'Test 31 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <uint foo = 0xfffffffffffffffflu;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 32 failed' : 'Test 32 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <uint foo = 0xffffffffffffffffUL;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 33 failed' : 'Test 33 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <uint foo = 0xffffffffffffffffuL;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 34 failed' : 'Test 34 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <uint foo = 040000000000;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 35 passed' : 'Test 35 failed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <uint foo = 037777777777;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 36 failed' : 'Test 36 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <uint foo = 02000000000000000000000u;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 37 passed' : 'Test 37 failed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <uint foo = 01777777777777777777777u;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 38 failed' : 'Test 38 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <uint foo = 01777777777777777777777U;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 39 failed' : 'Test 39 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <uint foo = 02000000000000000000000l;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 40 passed' : 'Test 40 failed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <uint foo = 01777777777777777777777l;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 41 failed' : 'Test 41 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <uint foo = 01777777777777777777777L;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 42 failed' : 'Test 42 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <uint foo = 02000000000000000000000ul;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 43 passed' : 'Test 43 failed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <uint foo = 01777777777777777777777ul;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 44 failed' : 'Test 44 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <uint foo = 01777777777777777777777ul;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 45 failed' : 'Test 45 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <uint foo = 01777777777777777777777lu;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 46 failed' : 'Test 46 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <uint foo = 01777777777777777777777UL;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 47 failed' : 'Test 47 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <uint foo = 01777777777777777777777uL;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 48 failed' : 'Test 48 passed')
