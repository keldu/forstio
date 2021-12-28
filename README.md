# kelgin

Asynchronous framework mostly inspired by [Capn'Proto](https://github.com/capnproto/capnproto) with the key difference of not
using Promises, but more reusable Pipelines/Conveyors. This introduces some challenges since I can't assume that only one
element gets passed along the chain, but it is managable. The advantage is that you have zero heap overhead by not recreating the chain after every use.  
The asynchronous interfaces are very similar to [Capn'Proto](https://github.com/capnproto/capnproto). At least similar to the 0.7.0 version.  
The other elements like schema, serialization, network interfaces are designed in another manner.  

Early stage of development. I am currently rewriting my software to find a good interface solution by checking if I am comfortable with the current design.  
Schema description has been rewritten once already and the current version is probably here to stay. The default container will be changed in the future, so
no parsing will be necessary with my binary format.  

# Dependencies  

You will need  

* A compiler (std=c++20) (g++/clang++)  
* scons  
* gnutls  

Currently the build script explicitly calls clang++ due to some occasional internal compiler error on archlinux with g++.  
Optional dependencies are  

* clang-format  

# Build  

Execute `scons`.  
It's that simple.  

`scons test` build the test cases.  
`scons format` formats the sources.  
`scons install` installs the library + headers locally.  
`scons all` to format the sources and build the library and the tests.  

# Async  

The main interface for async events is the ```Conveyor``` class.  
This class is the builder object for creating new nodes in the graph of relations. At the same time it stores the end nodes of this graph.  
Most of the times this async relationship graph is started by using ```newConveyorAndFeeder``` which return a connected input and output
class. You can provide the templated data to the feeder and if you have built up a graph with the Conveyor class, it will get passed along the
chain.  

It is necessary to create an ```EventLoop``` instance and activate it on the current thread by creating a ```WaitScope``` class before using the
Async features.  

## External events  

Since a lot of external events may occur which originate from the OS in some way, we require an additional class called ```EventPort```.  
You can create your async context by calling ```setupAsyncIo()``` which creates this OS dependent ```EventPort``` for you.  
Only the ```WaitScope``` has to be created afterwards to these classes active on the current thread.  
Most of the times you want to create the ```AsyncIoContext``` on the main thread while in the future other threads can or should have a custom implementation
of ```EventPort``` to allow for external events arriving for these threads as well. In the context of threads external means outside of the mentioned thread.  

Cross-Thread communication currently is implemented differently due to missing features. It is done either by implementing your own thread-safe data
transfer or using the network features to communicate across threads.  

# Schema Structure  

Message description currently is achieved by a series of templated schema classes found in ```kelgin/schema.h``` as seen below

```
using BasicStruct = schema::Struct<
	schema::NamedMember<Int32, "foo">,
	schema::NamedMember<String, "bar">	
>;
```  
These schema classes are just meant to describe the schema itself. By itself, it can't do anything.  
For a message we create an instance of any MessageRoot class such as `HeapMessageRoot`.  
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
