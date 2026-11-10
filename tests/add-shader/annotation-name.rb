# Test suite for Graphics.add_shader
# Author: white-axe (2026)

# Creative Commons CC0: To the extent possible under law, white-axe has
# dedicated all copyright and related and neighboring rights to this script
# to the public domain worldwide.
# https://creativecommons.org/publicdomain/zero/1.0/

# Run the suite via the "customScript" field in mkxp.json.

# A type name can be used as the name of an annotation.
error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <int int = 1;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 1 failed' : 'Test 1 passed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <int dword = 1;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 2 failed' : 'Test 2 passed')

# However, `string` and `String` cannot be used as annotation names since they're keywords.
# Other letter-cased variants of `string` can be used as annotation names.
error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <int string = 1;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 3 passed' : 'Test 3 failed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <int String = 1;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 4 passed' : 'Test 4 failed')

error_occurred = false
begin
    Graphics.add_shader(<<~END_SHADERS, <<~END_PASSES)
    END_SHADERS
        pass <int STRING = 1;> {}
    END_PASSES
rescue RGSSError
    error_occurred = true
end
System.puts(error_occurred ? 'Test 5 failed' : 'Test 5 passed')
