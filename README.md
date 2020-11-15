# kelgin

Asynchronous framework mostly inspired by [Capn'Proto](https://github.com/capnproto/capnproto) with the key difference of not
using Promises, but more reusable Pipelines/Conveyors. This introduces some challenges since I can't assume that only one
element gets passed along the chain, but it is managable.  

Very early stage. I am currently rewriting a lot of my software to use this library.  

# Dependencies  

You will need  

* A compiler (g++/clang++)  
* scons  

Optional dependencies are  

* clang-format  

# Build  

Execute `scons`.  
It's that simple.  

`scons test` build the test cases.  
`scons format` formats the sources.  
`scons install` installs the library + headers locally.  

# Examples  

Currently no examples except in test.  

# Roadmap  

* Zerocopy for message templates during parsing  
* Tls with gnutls  
* Windows/Mac Support  
* Buffer flexibility  
* Multithreaded conveyor communication  
