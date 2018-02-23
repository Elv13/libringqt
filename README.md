# LibRingQt

libringqt is a library for GNU Ring, a secure communication platform.

## Design

This library exposes about 40 QAbstractItemModels for various components of Ring.
It also tracks the state internally and take great care of always exposing up
to date and valid data.

It also provides a powerful extension system to integrate system specific assets
such as contacts, ringtones, TLS certificates into the system so `libringqt` can
track them automatically.

## Main concepts

Here are the main concepts of LibRingQt from the most abstract network entity
down to an actual human. Each layer of abstract exists to cleanly handle any
ambiguous obstacle, changes of cardinality or synchronization issues.

### Call

An audio, video or historical call.

### URI

Either a phone number, a SIP URI or an unique Ring account identifier.

### ContactMethod

How to reach someone and the associated metadata for that specific way of
reaching him/her/it.

### Individual

Either a contact, a peer or the user itself. This is a very abstract view of
someone. They always exists whether or not anything about the person is known.
It is a set of `ContactMethod` and the concatenation of their metadata.

*This object tracks the metadata associated with someone*

### Person

In concrete version of the individual. It has an undefined set of attributes.

*This object tracks the information associated with someone*


## Main interfaces

TODO (callable, schedulable, present/tracked, collectable)

## Supported platforms

 * Linux (amd64 and ARM)
 * macOS
 * Android
 * Windows (not tested as often, but known to work)

Qt 5.3+ is supported, Qt 5.6+ recommended.

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

## Credits

 * Copyright Savoir-faire Linux 2004-2017
 * Copyright Emmanuel Lepage-Vallee 2015-present
 * Copyright BlueSystems 2017-present
