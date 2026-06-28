#pragma once

#include "../container/ArchiveDictionary.h"
#include "../protocol/ItemHeader.h"

namespace ArchiveUtil {
using ClassItem = OLSA::Container::ClassItem;
using ModuleItem = OLSA::Container::ModuleItem;
}

using TaggedObject = OLSA::Container::TaggedObject;
namespace TypeCode = FTML::BinaryArchive::TypeCode;
using ItemHeader = FTML::BinaryArchive::ItemHeader;

