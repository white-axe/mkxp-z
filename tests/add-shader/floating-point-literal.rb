# Test suite for Graphics.add_shader
# Author: white-axe (2026)

# Creative Commons CC0: To the extent possible under law, white-axe has
# dedicated all copyright and related and neighboring rights to this script
# to the public domain worldwide.
# https://creativecommons.org/publicdomain/zero/1.0/

# Run the suite via the "customScript" field in mkxp.json.

# Floating-point literals can be any size.
error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <float foo = 999999999999999999999999999999999999999999999.999999999999999999999999999999999999999999999;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 1 failed' : 'Test 1 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <float foo = 999999999999999999999999999999999999999999999.999999999999999999999999999999999999999999999h;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 2 failed' : 'Test 2 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <float foo = 999999999999999999999999999999999999999999999.999999999999999999999999999999999999999999999H;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 3 failed' : 'Test 3 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <float foo = 999999999999999999999999999999999999999999999.999999999999999999999999999999999999999999999f;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 4 failed' : 'Test 4 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <float foo = 999999999999999999999999999999999999999999999.999999999999999999999999999999999999999999999F;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 5 failed' : 'Test 5 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <float foo = 999999999999999999999999999999999999999999999.999999999999999999999999999999999999999999999l;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 6 failed' : 'Test 6 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <float foo = 999999999999999999999999999999999999999999999.999999999999999999999999999999999999999999999L;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 7 failed' : 'Test 7 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <float foo = 999999999999999999999999999999999999999999999.999999999999999999999999999999999999999999999e999999999999999999999999999999999999999999999;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 8 failed' : 'Test 8 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <float foo = 999999999999999999999999999999999999999999999.999999999999999999999999999999999999999999999E999999999999999999999999999999999999999999999;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 9 failed' : 'Test 9 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <float foo = 999999999999999999999999999999999999999999999.999999999999999999999999999999999999999999999e+999999999999999999999999999999999999999999999;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 10 failed' : 'Test 10 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <float foo = 999999999999999999999999999999999999999999999.999999999999999999999999999999999999999999999e-999999999999999999999999999999999999999999999;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 11 failed' : 'Test 11 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <float foo = 999999999999999999999999999999999999999999999e999999999999999999999999999999999999999999999;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 12 failed' : 'Test 12 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <float foo = 999999999999999999999999999999999999999999999.;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 13 failed' : 'Test 13 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <float foo = 999999999999999999999999999999999999999999999.e999999999999999999999999999999999999999999999;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 14 failed' : 'Test 14 passed')
