# TODO

<!-- markdownlint-disable MD025 -->

## Ongoing

What primitives can we get rid of?

Should any library functions take advantage of other library functions or
primitives that they aren't?

Review code for conformance to the style guidelines

Look for redundant or unneeded library tests that we can remove to keep the
test suite size down.

## Regular

Change equal_operative to use op== and/or use other type-specific customization point

Tests for `raise`. Add error checking to library operatives.

More (type) predicates

map*

append, length, reverse

filter

Other HOF

During `define`, attach the name to the value directly for debugging?
Values can have a flat_set of names?
Include a (weak) reference to the environment?

TCO

First-class delimited continuations

Add expansion-time macros (see [having-both-fexprs-and-macros.html](https://axisofeval.blogspot.com/2012/09/having-both-fexprs-and-macros.html) )

* A `macro` primitive works kind of like `wrap` to turn any operative into
* a macro transformer.
* Need a macro transformer primitive that will expand macros.
* When the code is creating an operative, run macro expansion on the body before storing it in the operative.

There's a lot of common code that could be refactored in the builtin operatives

Pattern matching?

"Numeric tower"...or at least support of bignums

* All numbers are boost::cpp_rational
* Default formatting is (possibly) repeating decimals
* Floats will be boost::cpp_dec_float

Use demangle in value_to_string?

Revisit the uses of std::visit

Look for places we could simplify code with ranges

Revisit escaping in string literals (what scheme do we want to use?)

Replace lists with arrays?

> I'll avoid set-car!/set-cdr! for now...a set-array-element! might happen
> And that might be needed for a good promise implementation

Consider adding an equivalent to Kernel's #ignore

Module system?

Hash sets and maps

Interning symbols

Support for lazy evaluation (We can do this in the library, right?)

FFI and POSIX support

Look for places we can use string_view

Consider whether to adopt Kernel's $ naming convention

Would it make sense to implement `if` in terms of `cond`?

Implement void or #inert?

Dynamic variables (a la Kernel?) (could be used for test-failures in the library tests)

Consolidate write and display into a single primitive

Consider if*, cond*, and*, or*, not* library functions that can handle
non-boolean values

Rewrite any uses of if into Church Boolean form in the library

Do we need get-current-environment? Is it correct?

`for-each*` that can handle *n*-ary procedures?

Move the testing infrastructure into the library?
Or wait until we have modules and put it in a module?

# System calls

## Proposal 1

Use libclang to parse /usr/include/sys/syscall.h (and related headers) to
extract syscall numbers, parameter types, and return types. Generate the data
that Noeval will use for the bindings.

Create a single Noeval operative that takes syscall name/number and arguments
and uses libffi to make the call.

## Proposal 2

Use a language with system call integration and reflection as a system call
gateway.

Great question! Several options can provide system call metadata without manual parsing:

### Languages with Built-in System Call Metadata

#### Go's syscall package

Go has comprehensive metadata built into its standard library:

````go
package main

import (
    "encoding/json"
    "reflect"
    "syscall"
)

func describeSyscalls() map[string]interface{} {
    metadata := make(map[string]interface{})
    
    // Go's syscall package has typed wrappers
    t := reflect.TypeOf(syscall.Open)
    metadata["open"] = map[string]interface{}{
        "name": "open",
        "numIn": t.NumIn(),
        "numOut": t.NumOut(),
        "in": getParamTypes(t),
        "out": getReturnTypes(t),
    }
    
    return metadata
}

func getParamTypes(t reflect.Type) []string {
    var types []string
    for i := 0; i < t.NumIn(); i++ {
        types = append(types, t.In(i).String())
    }
    return types
}
````

### Rust with nix crate

The `nix` crate provides typed system call wrappers with full metadata:

````rust
use nix::unistd;
use serde_json::json;

fn describe_syscalls() -> serde_json::Value {
    json!({
        "open": {
            "params": ["path: &Path", "flags: OFlag", "mode: Option<Mode>"],
            "returns": "Result<RawFd>"
        },
        "read": {
            "params": ["fd: RawFd", "buf: &mut [u8]"],
            "returns": "Result<usize>"
        }
        // Rust's type system gives you this for free
    })
}
````

### Python with inspect + ctypes

Python can introspect C library signatures automatically:

````python
import ctypes
import ctypes.util
import inspect
from ctypes import CDLL, POINTER, c_int, c_char_p, c_size_t

# Define typed wrappers that ctypes can introspect
libc = CDLL(ctypes.util.find_library("c"))

# Set up typed function signatures
libc.open.argtypes = [c_char_p, c_int]
libc.open.restype = c_int

libc.read.argtypes = [c_int, POINTER(ctypes.c_char), c_size_t]
libc.read.restype = c_size_t

def describe_syscall(func_name):
    func = getattr(libc, func_name)
    return {
        'name': func_name,
        'arg_types': [arg._type_ for arg in func.argtypes] if func.argtypes else [],
        'return_type': func.restype._type_ if func.restype else 'int'
    }
````

### Node.js with ffi-napi pre-configured

You can bundle pre-typed definitions:

````javascript
const syscalls = {
    'open': {
        signature: ['int', ['string', 'int']],
        params: [{name: 'path', type: 'string'}, {name: 'flags', type: 'int'}],
        returns: 'int'
    },
    'read': {
        signature: ['size_t', ['int', 'pointer', 'size_t']],
        params: [
            {name: 'fd', type: 'int'}, 
            {name: 'buf', type: 'pointer'}, 
            {name: 'count', type: 'size_t'}
        ],
        returns: 'size_t'
    }
    // Could be generated from existing FFI definitions
};

module.exports = syscalls;
````

### Julia with Automatic C Header Parsing

Julia's `Clang.jl` can parse headers automatically at runtime:

````julia
using Clang

function parse_syscall_headers()
    # Parse system headers automatically
    headers = ["/usr/include/unistd.h", "/usr/include/fcntl.h"]
    
    ctx = DefaultContext()
    parse_headers(ctx, headers)
    
    syscalls = Dict()
    for func in get_functions(ctx)
        if startswith(string(func.name), target_syscalls)
            syscalls[string(func.name)] = Dict(
                "params" => [string(p.type) for p in func.params],
                "return_type" => string(func.return_type)
            )
        end
    end
    
    return syscalls
end
````

### Pre-built Metadata Databases

**syscall-info projects**: Some GitHub projects maintain JSON databases of system call signatures across platforms.

**strace source**: The strace project has machine-readable syscall definitions that could be extracted.

**Linux kernel UAPI**: Some kernel headers are designed to be machine-readable.

### Recommendation for Exploration

**Go** might be your best bet for avoiding parsing entirely - its standard library already has typed wrappers for most important system calls with full reflection support. You'd get the metadata "for free" and Go's JSON marshaling would make the gateway protocol trivial.

**Rust with nix** would be second choice - excellent type safety and the nix crate covers most POSIX calls with proper typing.

Both options give you the metadata through the language's type system rather than requiring any parsing of external sources.

Would you like me to elaborate on any of these approaches, or explore how the communication protocol might work with any specific option?

## Proposal 3

Looking at your TODO and the fexpr-based architecture, here are some creative expedient approaches:

### Shell Command Gateway

Since you already have string primitives, wrap shell commands directly:

````cpp
class shell_operative : public builtin_operative 
{
public:
    value call(const value& operands, const value& env) override 
    {
        auto args = extract_list(operands);
        validate_min_args(args, 1);
        
        std::string command = extract_string(eval(args[0], env));
        
        // Add safety prefix for syscall-like operations
        if (command.starts_with("syscall:")) {
            command = command.substr(8); // Remove "syscall:" prefix
        }
        
        FILE* pipe = popen(command.c_str(), "r");
        std::string result = read_pipe_output(pipe);
        int status = pclose(pipe);
        
        return make_list({make_int(status), make_string(result)});
    }
};
````

Then in Noeval:

```lisp
(shell "syscall: strace -e trace=openat -o /dev/null ls 2>&1")
(shell "syscall: python3 -c \"import os; print(os.open('/tmp/test', os.O_CREAT))\"")
```

### Perl One-Liner Gateway

Perl has excellent syscall support built-in:

````cpp
value perl_syscall(const std::string& syscall_name, const std::vector<std::string>& args) 
{
    std::ostringstream perl_cmd;
    perl_cmd << "perl -e 'print syscall(SYS_" << syscall_name;
    for (const auto& arg : args) {
        perl_cmd << ", " << shell_escape(arg);
    }
    perl_cmd << ")'";
    
    return execute_shell(perl_cmd.str());
}
````

### Ruby's FFI as Gateway

Ruby's DL/Fiddle library can call C functions dynamically:

````ruby
require 'fiddle'
require 'json'

libc = Fiddle.dlopen("libc.so.6")

def call_syscall(name, args)
    func = Fiddle::Function.new(
        libc[name],
        args.map { |a| guess_type(a) },
        Fiddle::TYPE_INT
    )
    
    puts JSON.dump({
        result: func.call(*args),
        errno: Fiddle.last_error
    })
end

ARGV.each_slice(2) do |name, *args|
    call_syscall(name, args)
end
````

### SQLite as Syscall Database

Use SQLite with a pre-populated syscall metadata table:

````sql
CREATE TABLE syscalls (
    name TEXT PRIMARY KEY,
    number INTEGER,
    arg_types TEXT, -- JSON array
    return_type TEXT
);

INSERT INTO syscalls VALUES 
('open', 2, '["string", "int", "int"]', 'int'),
('read', 0, '["int", "pointer", "size_t"]', 'ssize_t');
````

Then query from Noeval using sqlite3 command line.

### Docker Container Gateway

Run system calls in isolated containers:

````cpp
std::string docker_syscall(const std::string& syscall, const std::vector<std::string>& args) 
{
    std::ostringstream cmd;
    cmd << "docker run --rm alpine:latest sh -c \"";
    cmd << "python3 -c 'import os; print(os." << syscall << "(";
    
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) cmd << ", ";
        cmd << "\\\"" << args[i] << "\\\""; // Crude escaping
    }
    cmd << "))'\"";
    
    return execute_command(cmd.str());
}
````

### Web API Gateway

Create a simple HTTP service that handles syscalls:

````javascript
const express = require('express');
const fs = require('fs');

const app = express();
app.use(express.json());

app.post('/syscall/:name', (req, res) => {
    const { name } = req.params;
    const { args } = req.body;
    
    try {
        let result;
        switch(name) {
            case 'open':
                result = fs.openSync(args[0], args[1]);
                break;
            case 'read':
                const buffer = Buffer.alloc(args[2]);
                result = fs.readSync(args[0], buffer, 0, args[2], null);
                break;
        }
        res.json({ result, errno: 0 });
    } catch (err) {
        res.json({ result: -1, errno: err.errno });
    }
});

app.listen(3000);
````

Then from Noeval, use curl or similar.

### Lua Script Embedding

Embed LuaJIT directly in your C++ process:

````cpp
#include <lua.hpp>

class lua_syscall_operative : public builtin_operative 
{
    lua_State* L;
    
public:
    lua_syscall_operative() {
        L = luaL_newstate();
        luaL_openlibs(L);
        
        // Load syscall definitions
        luaL_dostring(L, R"(
            local ffi = require("ffi")
            ffi.cdef[[
                int open(const char *path, int flags);
                int read(int fd, void *buf, size_t count);
            ]]
            
            function syscall(name, ...)
                return ffi.C[name](...)
            end
        )");
    }
    
    value call(const value& operands, const value& env) override {
        // Call Lua syscall function with reflection
        // Return result to Noeval
    }
};
````

### Most Expedient: Shell + jq

Combine shell commands with jq for structured data:

````bash
#!/bin/bash
# filepath: syscall_wrapper.sh

case "$1" in
    "describe")
        echo '{"open": {"args": ["string", "int"], "returns": "int"}}'
        ;;
    "call")
        # Use strace to intercept and format results
        strace -e trace="$2" -o /tmp/trace "$3" "${@:4}" 2>/dev/null
        grep "$2" /tmp/trace | tail -1 | jq -c '{result: .return_value}'
        ;;
esac
````

### Recommendation for Maximum Expediency

The **shell + Python one-liner** approach might be fastest to implement:

````cpp
value call(const value& operands, const value& env) override 
{
    auto args = extract_list(operands);
    std::string syscall_name = extract_string(eval(args[0], env));
    
    std::ostringstream cmd;
    cmd << "python3 -c \"import os, json; ";
    cmd << "result = os." << syscall_name << "(";
    
    for (size_t i = 1; i < args.size(); ++i) {
        if (i > 1) cmd << ", ";
        auto arg = eval(args[i], env);
        if (holds_int(arg)) cmd << extract_int(arg);
        else cmd << "\\\"" << extract_string(arg) << "\\\"";
    }
    
    cmd << "); print(json.dumps({'result': result, 'errno': 0}))\"";
    
    return parse_json_result(execute_shell(cmd.str()));
}
````

This gives you immediate access to Python's `os` module syscalls with minimal implementation effort, and you can always optimize later.

Would any of these creative shortcuts align with your exploration goals?
