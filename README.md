# kelgin

Asynchronous framework mostly inspired by [Capn'Proto](https://github.com/capnproto/capnproto) with the key difference of not
using Promises, but more reusable Pipelines/Conveyors. This introduces some challenges since I can't assume that only one
element gets passed along the chain, but it is managable. The advantage is that you have zero heap overhead by recreating the chain after every use.  

Very early stage. I am currently rewriting my software to find a good interface solution by checking if I am comfortable with the current design.  

# Dependencies  

You will need  

* A compiler (std=c++20) (g++/clang++)  
* scons  
* gnutls  

Currently the build script explicitly calls clang++ due to some occasional compiler error on archlinux with g++.  
Optional dependencies are  

* clang-format  

# Build  

Execute `scons`.  
It's that simple.  

`scons test` build the test cases.  
`scons format` formats the sources.  
`scons install` installs the library + headers locally.  

# Schema Structure  

Message description currently is achieved by a series of templated schema classes found in ```kelgin/schema.h``` as seen below

```
using BasicStruct = schema::Struct<
	schema::NamedMember<Int32, "foo">,
	schema::NamedMember<String, "bar">	
>;
```  
These schema classes are just meant to describe the schema itself. By itself, it can't do anything.  
For a message we build 
Using those schemas and appropriate container classes, we can now build a message class

```
HeapMessageRoot<BasicStruct, MessageContainer<BasicStruct>> buildBasicMessage(){
	auto root = heapMessageRoot<BasicStruct>();
	// This is equivalent to
	// auto root = heapMessageRoot<BasicStruct, MessageContainer<BasicStruct>>();

	auto builder = root.build();
	auto bar_build = builder.init<"bar">().set("banana");
	
	auto foo_build = builder.init<"foo">().set(5);

	return root;
}
```

The current default message container stores each value in stl containers as `std::string`, `std::vector`, `std::tuple`, `std::variant`
or in its primitive form.  
Though it is planned to allow storing those directly in buffers.  

# Examples  

Currently no examples except in test.  
But [kelgin-graphics](https://github.com/keldu/kelgin-graphics) contains some programs which heavily use
this library. Though no schema or io features are used there.  

# Roadmap  

* Zerocopy for message templates during parsing  
* Tls with gnutls (Client side partly done. Server side missing)  
* Windows/Mac Support  
* Multithreaded conveyor communication  
* Logger implementation  
* Reintroduce JSON without dynamic message parsing or at least with more streaming support  
