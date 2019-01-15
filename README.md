# LibRingQt

libringqt is a library for GNU Ring, a secure communication platform.

It provides a full middleware personal information management system along with
"turn key" QML and Qt Model/View bindings to make creating Qt+Ring.cx
applications quick and safe.

## Design

This library exposes about 40 QAbstractItemModels for various components of Ring.
It also tracks the state internally and take great care of always exposing up
to date and valid data.

It also provides a powerful extension system to integrate system specific assets
such as contacts, ringtones, TLS certificates into the system so `libringqt` can
track them automatically.

The API aims to be externally consistent and internally implemented as a set
of state machines. The public API only exposes already validated data and
prevent the state from being corrupted. Most public classes have an internal
API only exposed to `friend class`.

## Main concepts

Here are the main concepts of LibRingQt from the most abstract network entity
down to an actual human. Each layer of abstract exists to cleanly handle any
ambiguous obstacle, changes of cardinality or synchronization issues.

### Call

An audio, video or historical call. This class is slowly deprecated in the API
in favor of `Event` and will eventually be merged with the `Media` subsystem
once it is no longer coupled with everything else.

### Event / Calendar

A call, chat session, file transfer, voice mail or anything else that has
a number of participant, a point in time where it starts and a point in time
where it ends. This still being integrated and is a work in progress. It aims to
help uncouple some of the phone related concepts and help migrate to a more
generic multimedia / PIM platform as the scope is extended.

Groups of event are managed as Calendar objects. This is intended to be extended
for an archival and cross device synchronization subsystem.

### URI

Either a phone number, a SIP URI or an unique Ring account identifier.

### ContactMethod

How to reach someone and the associated metadata for that specific way of
reaching him/her/it. It has the following primary keys:

 * An URI (immutable)
 * An Account object (can be set only once)
 * A Person object (can be set only once)

The factory is `IndividualDirectory` and will take care of re-using and
de-duplicating all instances.

A `TemporaryContactMethod` subclass exists with more related requirements but
can only be used to build "true" ContactMethods (like in the search model).

This class instances are heavily indexed and the lookup is very fast.

### Individual

Either a contact, a peer or the user itself. This is a very abstract view of
someone. They always exists whether or not anything about the person is known.
It is a **set** of `ContactMethod` and the concatenation of their metadata.

*This object tracks the metadata associated with someone*

### Person

In concrete version of the individual. It has an undefined number of attributes.

*This object tracks the information associated with someone*


## Main interfaces

TODO (callable, schedulable, present/tracked, collectable)

## Supported platforms

 * Linux (amd64 and ARM)
 * macOS
 * Android
 * Windows (not tested as often, but known to work)

Qt 5.3+ is supported, Qt 5.6+ recommended.

## Main subsystems

### Media

The media namespace is where all classes related to live telecommunication
classes reside.

### Troubleshoot

A set of live problem solving algorithms intended to restore a coherent state
when something goes wrong. Ring is always a distributed system and SIP is
often an unreliable federation network. Add the use of UDP and you end up with
a very fragile system that *will* eventually fail.

### Libcard

Libcard is a semi independant component. It implements the VObject format as
used by various IEFT Internet standards such as vCard or iCal. It uses modern
C++ instead of Qt where it makes sense. While it's still quite coupled with
libringqt clases, it aims at being it's own thing as much as possible.

### Libcollection

The collection subsystem is a generic C++ CMS system to manage various assets
on various platform. It is pluggable and allows to interact with third party
systems using a standardized interface. There is contact collections for KDE
Akonadi, macOS addressbook, vCard folders, Gnome Evolution. It's also used for
TLS certificates, ringtones, calendars, blockchains, caching, voicemails and
alsmost every other serializable classes.

### QtWrapper / DBUS / Rest

They implement various IPC mechanism to talk to LibRing. QtWrapper just converts
stdlib types into Qt types and link directly to LibRing. DBus and Rest use
multiprocess message passing.

Both DBus and "qtwrapper" modes are currently officially supported. REST would
eventually be nice and a msgpack mode would also be interesting for sandboxing.

## History

This library has been created to split the logic and GUI code of SFLPhone-KDE,
a software phone. It was forked into LibRingClient then Ring-LRC when the
SFLPhone application was EOLed and the GNU Ring project arose from its ashes.

It was moved from the KDE project to the GNU project. During the GNU era, the
focus on Qt was dropped and a fork was necessary to keep alive the Qt APIs and
models. The upstream GNU Ring project deprecated all relevant API used to
create QML bindings. This fork is a community project and is no longer developed
byu Savoir-faire Linux, the sole author of GNU Ring. It is maintained by one of
the two original author of the code and sponsored by Blue Systems.

This project aims to continue the development of a QtCore based library with
full QML compatible bindings for the GNU Ring project.

It's API is currently still unstable as the fork offers an unique opportunity
to port the API to C++14 and get rid of old and unused components. Once the
new API is finalized, this project will attempt to re-join the KDE project.

## Memory management

This library memory management is a mix of legacy limitations and odd design
requirements. Up until Qt4 support was dropped, C++11 wasn't usable along with
Qt and this caused the proliferation if little classes to link everything. Up
to this day, QML support for smart pointer is horrible and thus everything has
to be exposed as raw pointers to be reliable (along with never using `const*`.
This make it nearly impossible to delete the main objects.

To work around this and help the metadata "converge" to a smaller number of
instances, there is online deduplication of most important objects. Factory
classes are responsible of creating new instances, re-using old one and merging
instances when all ambiguity is gone. Merging is done by using the object
PIMPLE pointers (`d_ptr`). By deleting the d_ptr, it is possible to keep the
memory usage mostly stable while never deliting the main pointers.

The problem with that is that signals are proxyed to all top-level objects
and `==`/`!=` cannot safely be used to compare pointers (as they might proxy the
same "real" object). Unfortunately, it is also used in hundreds of places where
it should not.

As a side effect, `delete` is used on the `d_ptr` instead of Qt own object tree
to avoid accidentally deleting objects while one of their proxy is still alive.

Objects that are not intended to be exposed to the bindings use shared pointers.
However if they ever become exposed to the bindings, it has to be reverted and
this has so far caused a lot of problem (like converting `QWeakPointer` to
`QSharedPointer` so the memory is in effect never freed and `.data()` becomes
safe.

## Credits

 * Author Emmanuel Lepage-Vallee
 * Author Stepan Salenikovich
 * Copyright Savoir-faire Linux 2004-2017
 * Copyright Emmanuel Lepage-Vallee 2015-present
 * Copyright BlueSystems 2017-present
