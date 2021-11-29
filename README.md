# kelgin

Asynchronous framework mostly inspired by [Capn'Proto](https://github.com/capnproto/capnproto) with the key difference of not
using Promises, but more reusable Pipelines/Conveyors. This introduces some challenges since I can't assume that only one
element gets passed along the chain, but it is managable. The advantage is that you have zero heap overhead by recreating the chain after every use.  

Very early stage. I am currently rewriting my software to find a good interface solution by checking if I am comfortable with the current design.  

# Dependencies  

You will need  

* A compiler (std=c++17) (g++/clang++)  
* scons  
* gnutls  

Optional dependencies are  

* clang-format  

# Build  

Execute `scons`.  
It's that simple.  

`scons test` build the test cases.  
`scons format` formats the sources.  
`scons install` installs the library + headers locally.  

# Schema Structure  

Message description currently is achieved by a series of templated Message classes as seen below.  
Though it is sufficient the next code block is a little bit tedious to write.  

```
using BasicStruct = MessageStruct<
	MessageStructMember<decltype("foo"_t), MessagePrimitive<int32_t>,
	MessageStructMember<decltype("bar"_t), MessagePrimitive<std::string>
>;
```  
  
Since this concept unites writing the schema description coupled with the storage in memory I have chosen to seperate these concepts into
schema descriptions and containers. The Message class would be just a wrapper around those concepts. Together with C++20 templated string literals and
the seperated schema and variable storage we achieve a description in this style.  

```
using BasicStruct = schema::Struct<
	schema::NamedMember<"foo", Int32>,
	schema::NamedMember<"bar", String>	
>;
```  

The main advantage is the increased readibility. Together with the seperated storage we are now able to implement zerocopy storage patterns.  
This message description change is currently being developed and the old message description / schema classes will be removed when it is finished.  

# Examples  

Currently no examples except in test.  
But [kelgin-graphics](https://github.com/keldu/kelgin-graphics) contains some programs which heavily use
this library. Though no schema or io features are used there.  

# Roadmap  

* Zerocopy for message templates during parsing  
* Tls with gnutls  
* Windows/Mac Support  
* Multithreaded conveyor communication  
* Logger implementation  
