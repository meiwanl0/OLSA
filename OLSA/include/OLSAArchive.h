#pragma once

#include "Archive.h"
#include "BinaryArchive.h"
#include "BinaryFileReader.h"
#include "SmartPointer.h"
#include "TextArchive.h"
#include "XMSERawDataSet.h"

#include "container/ArchiveDictionary.h"
#include "container/FileFormat.h"
#include "domain/FTMLBase.h"
#include "domain/XMSERawDataSet.h"
#include "protocol/ItemHeader.h"
#include "protocol/TypeCode.h"

namespace OLSA {
using Archive = ::Archive;
using BinaryArchive = ::BinaryArchive;
using BinaryFileReader = ::BinaryFileReader;
using TextArchive = ::TextArchive;
}

namespace OLSA::Protocol {
using ItemHeader = FTML::BinaryArchive::ItemHeader;
namespace TypeCode = FTML::BinaryArchive::TypeCode;
}
