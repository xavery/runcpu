# runcpu

`runcpu` is a program that can be used for launching Windows processes with a
pre-set affinity and working directory.

Running the program goes more or less like this :
```
runcpu -d "D:\working_directory" -a 3 -- D:\foo.exe -arg1="foo bar" -arg2="bar foo"
```

Such an invocation will run `D:\foo.exe` with the affinity set to 0x3 (so, the
process will be allowed to run on CPU 0 and CPU 1), and the working directory
set to `D:\working_directory`. Parameters `-arg1="foo bar" -arg2="bar foo"` are
passed verbatim into `D:\foo.exe`.

I originally wrote this tool because I wanted to spawn a number of virtual
machines on my Windows XP computer, with each of them bound tightly to only one
physical CPU. For some reason, I couldn't get it to work with standard Windows
utilities, so I decided to try it on my own.

I doubt whether this is of any use to anyone at the moment, since I'm sure doing
something like that is possible with PowerShell, which is much more suitable to
such tasks.
