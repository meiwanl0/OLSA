#pragma once

#include "../container/FileFormat.h"

using Header = OLSA::Container::Header;
using Node = OLSA::Container::Node;
using DirEntry = OLSA::Container::DirEntry;

static_assert(sizeof(Header) == 0x58, "Header size mismatch");
static_assert(sizeof(Node) == 0x10, "Node size mismatch");
static_assert(sizeof(DirEntry) == 0x24, "DirEntry size mismatch");

