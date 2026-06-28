#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "ByteUtil.h"
#include "OLSAArchive.h"

namespace FTML::XMSE {
    

    struct DCRecipe {
        std::uint32_t numCyclesRef{};
        std::uint32_t numCyclesDark{};
        std::uint32_t numCyclesSample{};

        bool sumCycles{};

        bool wRangeHasMin{};
        bool wRangeHasMax{};
        float wRangeMin{};
        float wRangeMax{};

        float analyzerRef{};
        float analyzerSample{};
        float rotation{};

        bool symThreshHas{};
        float symThreshValue{};

        bool sumsPerCycleHas{};
        std::uint32_t sumsPerCycle{};

        bool timingModeHas{};
        std::uint32_t timingMode{};

        bool saturationHas{};
        std::uint32_t saturation{};
    };

    struct DPRecipe {
        std::int32_t binning{};

        bool applyMultiScanErr{};
        bool applyPSF{};
        bool applyLinearity{};
        bool applyDCOffset{};
        bool applyA0P0Offset{};
        bool applyWShift{};
        bool applyTilt{};
        bool applyIDN{};

        bool modelTilt{};
    };

    struct RawData {
        std::uint32_t numSums{};
        std::uint32_t sumsPerCycle{};
        std::uint32_t timingMode{};
        std::int32_t numPixel{};
        std::uint32_t turnsPerCycle0{};
        std::uint32_t turnsPerCycle1{};
        std::uint32_t numBM{};
        std::uint32_t firstSum{};
        std::uint32_t firstAcqSum{};

        bool pixelRangeHasMin{};
        bool pixelRangeHasMax{};
        std::int32_t pixelRangeMin{};
        std::int32_t pixelRangeMax{};

        std::uint32_t clkPeriod{};
        std::uint32_t enc1Lines{};
        std::uint32_t enc2Lines{};
    };

    namespace {
        static bool GetXMSEConfiguration(Archive& ar, Configuration& out) {

            FTML::DC::GetDCConfiguration(ar, out.dcConfig);
            // FTML::DC::Configuration::get

            // FTML::DC::Configuration::get

            // FTML::Archive::operator()(a2, "SumsPerCycle");
            if(!ar.get(TypeCode::ID::UInt32, 1)){
                return false;
            }
            else {
                out.sumsPerCycle = read_little_endian<std::uint32_t>(as_bytes(ar.state().parsing.itemHeader.payloadBytes));
            }

            // FTML::Archive::operator()(a2, "TimingMode");
            if(!ar.get(TypeCode::ID::UInt32, 1)){
                return false;
            }
            else {
                out.timingMode = read_little_endian<std::uint32_t>(as_bytes(ar.state().parsing.itemHeader.payloadBytes));
            }

            // FTML::Archive::operator()(a2, "Saturation");
            if(!ar.get(TypeCode::ID::UInt32, 1)){
                return false;
            }
            else {
                out.saturation = read_little_endian<std::uint32_t>(as_bytes(ar.state().parsing.itemHeader.payloadBytes));
            }

            // FTML::Archive::operator()(a2, "TurnsPerCycle");
            if(!ar.get(TypeCode::ID::UInt32, 1)){
                return false;
            }
            else {
                out.turnsPerCycle0 = read_little_endian<std::uint32_t>(as_bytes(ar.state().parsing.itemHeader.payloadBytes));
            }
            if(!ar.get(TypeCode::ID::UInt32, 1)){
                return false;
            }
            else {
                out.turnsPerCycle1 = read_little_endian<std::uint32_t>(as_bytes(ar.state().parsing.itemHeader.payloadBytes));
            }

            // FTML::Archive::operator()(a2, "NumPixel");
            if(!ar.get(TypeCode::ID::Int32, 1)){
                return false;
            }
            else {
                out.numPixel = read_little_endian<std::int32_t>(as_bytes(ar.state().parsing.itemHeader.payloadBytes));
            }

            return true;
        }

        static bool GetXMSEDCRecipe(Archive& ar, DCRecipe& out) {
            // FTML::DC::DCRecipe::get(this, a2);

            // FTML::Archive::operator()(a2, "HWSRef");
            // FTML::HWSetupDef::get(...)

            // FTML::Archive::operator()(a2, "HWSDark");
            // FTML::HWSetupDef::get(...)

            // FTML::Archive::operator()(a2, "HWSSample");
            // FTML::HWSetupDef::get(...)

            // FTML::Archive::operator()(a2, "NumCyclesRef");
            if(!ar.get(TypeCode::ID::UInt32, 1)){
                return false;
            }
            else {
                out.numCyclesRef = read_little_endian<std::uint32_t>(as_bytes(ar.state().parsing.itemHeader.payloadBytes));
            }

            // FTML::Archive::operator()(a2, "NumCyclesDark");
            if(!ar.get(TypeCode::ID::UInt32, 1)){
                return false;
            }
            else {
                out.numCyclesDark = read_little_endian<std::uint32_t>(as_bytes(ar.state().parsing.itemHeader.payloadBytes));
            }

            // FTML::Archive::operator()(a2, "NumCyclesSample");
            if(!ar.get(TypeCode::ID::UInt32, 1)){
                return false;
            }
            else {
                out.numCyclesSample = read_little_endian<std::uint32_t>(as_bytes(ar.state().parsing.itemHeader.payloadBytes));
            }

            // FTML::Archive::operator()(a2, "SumCycles");
            if(!ar.get(TypeCode::ID::Bool, 1)){
                return false;
            }
            else {
                out.sumCycles = read_little_endian<std::uint16_t>(as_bytes(ar.state().parsing.itemHeader.payloadBytes)) != 0;
            }

            // FTML::Archive::operator()(a2, "WRange");
            if(!ar.get(TypeCode::ID::Bool, 1)){
                return false;
            }
            else {
                out.wRangeHasMin = read_little_endian<std::uint16_t>(as_bytes(ar.state().parsing.itemHeader.payloadBytes)) != 0;
            }
            if(!ar.get(TypeCode::ID::Bool, 1)){
                return false;
            }
            else {
                out.wRangeHasMax = read_little_endian<std::uint16_t>(as_bytes(ar.state().parsing.itemHeader.payloadBytes)) != 0;
            }
            if(out.wRangeHasMin){
                if(!ar.get(TypeCode::ID::Float, 1)){
                    return false;
                }
                else {
                    out.wRangeMin = read_little_endian<float>(as_bytes(ar.state().parsing.itemHeader.payloadBytes));
                }
            }
            if(out.wRangeHasMax){
                if(!ar.get(TypeCode::ID::Float, 1)){
                    return false;
                }
                else {
                    out.wRangeMax = read_little_endian<float>(as_bytes(ar.state().parsing.itemHeader.payloadBytes));
                }
            }

            // FTML::Archive::operator()(a2, "AnalyzerRef");
            if(!ar.get(TypeCode::ID::Float, 1)){
                return false;
            }
            else {
                out.analyzerRef = read_little_endian<float>(as_bytes(ar.state().parsing.itemHeader.payloadBytes));
            }

            // FTML::Archive::operator()(a2, "AnalyzerSample");
            if(!ar.get(TypeCode::ID::Float, 1)){
                return false;
            }
            else {
                out.analyzerSample = read_little_endian<float>(as_bytes(ar.state().parsing.itemHeader.payloadBytes));
            }

            // FTML::Archive::operator()(a2, "Rotation");
            if(!ar.get(TypeCode::ID::Float, 1)){
                return false;
            }
            else {
                out.rotation = read_little_endian<float>(as_bytes(ar.state().parsing.itemHeader.payloadBytes));
            }

            // FTML::Archive::operator()(a2, "SymThresh");
            if(!ar.get(TypeCode::ID::Bool, 1)){
                return false;
            }
            else {
                out.symThreshHas = read_little_endian<std::uint16_t>(as_bytes(ar.state().parsing.itemHeader.payloadBytes)) != 0;
                if(out.symThreshHas){
                    if(!ar.get(TypeCode::ID::Float, 1)){
                        return false;
                    }
                    else {
                        out.symThreshValue = read_little_endian<float>(as_bytes(ar.state().parsing.itemHeader.payloadBytes));
                    }
                }
            }

            // FTML::Archive::operator()(a2, "SumsPerCycle");
            if(!ar.get(TypeCode::ID::Bool, 1)){
                return false;
            }
            else {
                out.sumsPerCycleHas = read_little_endian<std::uint16_t>(as_bytes(ar.state().parsing.itemHeader.payloadBytes)) != 0;
                if(out.sumsPerCycleHas){
                    if(!ar.get(TypeCode::ID::UInt32, 1)){
                        return false;
                    }
                    else {
                        out.sumsPerCycle = read_little_endian<std::uint32_t>(as_bytes(ar.state().parsing.itemHeader.payloadBytes));
                    }
                }
            }

            // FTML::Archive::operator()(a2, "TimingMode");
            if(!ar.get(TypeCode::ID::Bool, 1)){
                return false;
            }
            else {
                out.timingModeHas = read_little_endian<std::uint16_t>(as_bytes(ar.state().parsing.itemHeader.payloadBytes)) != 0;
                if(out.timingModeHas){
                    if(!ar.get(TypeCode::ID::UInt32, 1)){
                        return false;
                    }
                    else {
                        out.timingMode = read_little_endian<std::uint32_t>(as_bytes(ar.state().parsing.itemHeader.payloadBytes));
                    }
                }
            }

            // FTML::Archive::operator()(a2, "Saturation");
            if(!ar.get(TypeCode::ID::Bool, 1)){
                return false;
            }
            else {
                out.saturationHas = read_little_endian<std::uint16_t>(as_bytes(ar.state().parsing.itemHeader.payloadBytes)) != 0;
                if(out.saturationHas){
                    if(!ar.get(TypeCode::ID::UInt32, 1)){
                        return false;
                    }
                    else {
                        out.saturation = read_little_endian<std::uint32_t>(as_bytes(ar.state().parsing.itemHeader.payloadBytes));
                    }
                }
            }

            // FTML::Archive::operator()(a2, "FilterRef");
            // FTML::NVT::NameString::get(...)

            // FTML::Archive::operator()(a2, "FilterSample");
            // FTML::NVT::NameString::get(...)

            // if (FTML::Archive::version(a2, 0xDCF62C00) > 1) { ... AverageDark / ValidityFlag ... }

            return true;
        }

        static bool GetXMSEDPRecipe(Archive& ar, DPRecipe& out) {
            // FTML::Archive::version(a2, 0xDCF65100);
            // FTML::DC::DPRecipe::get(...)

            // FTML::Archive::operator()(a2, "Binning");
            // FTML::XMSE::operator>>(...) 可能为 char(enum) 或 int32
            if(!ar.get(TypeCode::ID::Int32, 1)){
                return false;
            }
            else {
                out.binning = read_little_endian<std::int32_t>(as_bytes(ar.state().parsing.itemHeader.payloadBytes));
            }

            // if ( ... ) FTML::Archive::get(a2, 0xD, 1, ... );

            // FTML::Archive::operator()(a2, "ApplyMultiScanErr");
            if(!ar.get(TypeCode::ID::Bool, 1)){
                return false;
            }
            else {
                out.applyMultiScanErr = read_little_endian<std::uint16_t>(as_bytes(ar.state().parsing.itemHeader.payloadBytes)) != 0;
            }

            // FTML::Archive::operator()(a2, "ApplyPSF");
            if(!ar.get(TypeCode::ID::Bool, 1)){
                return false;
            }
            else {
                out.applyPSF = read_little_endian<std::uint16_t>(as_bytes(ar.state().parsing.itemHeader.payloadBytes)) != 0;
            }

            // FTML::Archive::operator()(a2, "ApplyLinearity");
            if(!ar.get(TypeCode::ID::Bool, 1)){
                return false;
            }
            else {
                out.applyLinearity = read_little_endian<std::uint16_t>(as_bytes(ar.state().parsing.itemHeader.payloadBytes)) != 0;
            }

            // FTML::Archive::operator()(a2, "ApplyDCOffset");
            if(!ar.get(TypeCode::ID::Bool, 1)){
                return false;
            }
            else {
                out.applyDCOffset = read_little_endian<std::uint16_t>(as_bytes(ar.state().parsing.itemHeader.payloadBytes)) != 0;
            }

            // FTML::Archive::operator()(a2, "ApplyA0P0Offset");
            if(!ar.get(TypeCode::ID::Bool, 1)){
                return false;
            }
            else {
                out.applyA0P0Offset = read_little_endian<std::uint16_t>(as_bytes(ar.state().parsing.itemHeader.payloadBytes)) != 0;
            }

            // FTML::Archive::operator()(a2, "ApplyWShift");
            if(!ar.get(TypeCode::ID::Bool, 1)){
                return false;
            }
            else {
                out.applyWShift = read_little_endian<std::uint16_t>(as_bytes(ar.state().parsing.itemHeader.payloadBytes)) != 0;
            }

            // FTML::Archive::operator()(a2, "ApplyTilt");
            if(!ar.get(TypeCode::ID::Bool, 1)){
                return false;
            }
            else {
                out.applyTilt = read_little_endian<std::uint16_t>(as_bytes(ar.state().parsing.itemHeader.payloadBytes)) != 0;
            }

            // FTML::Archive::operator()(a2, "ApplyIDN");
            if(!ar.get(TypeCode::ID::Bool, 1)){
                return false;
            }
            else {
                out.applyIDN = read_little_endian<std::uint16_t>(as_bytes(ar.state().parsing.itemHeader.payloadBytes)) != 0;
            }

            // version 分支：ModelA0P0Offset / ModelA0Offset / ModelP0Offset / ModelC0Offset / ApplyGOFAdjustCal / UseNormalizedAB ...

            // FTML::Archive::operator()(a2, "ModelTilt");
            if(!ar.get(TypeCode::ID::Bool, 1)){
                return false;
            }
            else {
                out.modelTilt = read_little_endian<std::uint16_t>(as_bytes(ar.state().parsing.itemHeader.payloadBytes)) != 0;
            }

            // FTML::Archive::operator()(a2, "FixedNoiseStack");
            // FTML::SmartPointer::extract(...)

            return true;
        }

        static bool GetXMSERawData(Archive& ar, RawData& out) {
            // FTML::Countable::get(this, a2);

            // FTML::Archive::operator()(a2, "HWSetup");
            // FTML::HWSetupDef::get(...)

            // FTML::Archive::operator()(a2, "NumSums");
            if(!ar.get(TypeCode::ID::UInt32, 1)){
                return false;
            }
            else {
                out.numSums = read_little_endian<std::uint32_t>(as_bytes(ar.state().parsing.itemHeader.payloadBytes));
            }

            // FTML::Archive::operator()(a2, "SumsPerCycle");
            if(!ar.get(TypeCode::ID::UInt32, 1)){
                return false;
            }
            else {
                out.sumsPerCycle = read_little_endian<std::uint32_t>(as_bytes(ar.state().parsing.itemHeader.payloadBytes));
            }

            // FTML::Archive::operator()(a2, "TimingMode");
            if(!ar.get(TypeCode::ID::UInt32, 1)){
                return false;
            }
            else {
                out.timingMode = read_little_endian<std::uint32_t>(as_bytes(ar.state().parsing.itemHeader.payloadBytes));
            }

            // FTML::Archive::operator()(a2, "NumPixel");
            if(!ar.get(TypeCode::ID::Int32, 1)){
                return false;
            }
            else {
                out.numPixel = read_little_endian<std::int32_t>(as_bytes(ar.state().parsing.itemHeader.payloadBytes));
            }

            // FTML::Archive::operator()(a2, "TurnsPerCycle");
            if(!ar.get(TypeCode::ID::UInt32, 1)){
                return false;
            }
            else {
                out.turnsPerCycle0 = read_little_endian<std::uint32_t>(as_bytes(ar.state().parsing.itemHeader.payloadBytes));
            }
            if(!ar.get(TypeCode::ID::UInt32, 1)){
                return false;
            }
            else {
                out.turnsPerCycle1 = read_little_endian<std::uint32_t>(as_bytes(ar.state().parsing.itemHeader.payloadBytes));
            }

            // FTML::Archive::operator()(a2, "NumBM");
            if(!ar.get(TypeCode::ID::UInt32, 1)){
                return false;
            }
            else {
                out.numBM = read_little_endian<std::uint32_t>(as_bytes(ar.state().parsing.itemHeader.payloadBytes));
            }

            // FTML::Archive::operator()(a2, "FirstSum");
            if(!ar.get(TypeCode::ID::UInt32, 1)){
                return false;
            }
            else {
                out.firstSum = read_little_endian<std::uint32_t>(as_bytes(ar.state().parsing.itemHeader.payloadBytes));
            }

            // FTML::Archive::operator()(a2, "FirstAcqSum");
            if(!ar.get(TypeCode::ID::UInt32, 1)){
                return false;
            }
            else {
                out.firstAcqSum = read_little_endian<std::uint32_t>(as_bytes(ar.state().parsing.itemHeader.payloadBytes));
            }

            // FTML::Archive::operator()(a2, "TimeStamp");
            // FTML::TimeStamp::get(...)

            // FTML::Archive::operator()(a2, "PixelRange");
            if(!ar.get(TypeCode::ID::Bool, 1)){
                return false;
            }
            else {
                out.pixelRangeHasMin = read_little_endian<std::uint16_t>(as_bytes(ar.state().parsing.itemHeader.payloadBytes)) != 0;
            }
            if(!ar.get(TypeCode::ID::Bool, 1)){
                return false;
            }
            else {
                out.pixelRangeHasMax = read_little_endian<std::uint16_t>(as_bytes(ar.state().parsing.itemHeader.payloadBytes)) != 0;
            }
            if(out.pixelRangeHasMin){
                if(!ar.get(TypeCode::ID::Int32, 1)){
                    return false;
                }
                else {
                    out.pixelRangeMin = read_little_endian<std::int32_t>(as_bytes(ar.state().parsing.itemHeader.payloadBytes));
                }
            }
            if(out.pixelRangeHasMax){
                if(!ar.get(TypeCode::ID::Int32, 1)){
                    return false;
                }
                else {
                    out.pixelRangeMax = read_little_endian<std::int32_t>(as_bytes(ar.state().parsing.itemHeader.payloadBytes));
                }
            }

            // FTML::Archive::operator()(a2, "ClkPeriod");
            if(!ar.get(TypeCode::ID::UInt32, 1)){
                return false;
            }
            else {
                out.clkPeriod = read_little_endian<std::uint32_t>(as_bytes(ar.state().parsing.itemHeader.payloadBytes));
            }

            // FTML::Archive::operator()(a2, "Enc1Lines");
            if(!ar.get(TypeCode::ID::UInt32, 1)){
                return false;
            }
            else {
                out.enc1Lines = read_little_endian<std::uint32_t>(as_bytes(ar.state().parsing.itemHeader.payloadBytes));
            }

            // FTML::Archive::operator()(a2, "Enc2Lines");
            if(!ar.get(TypeCode::ID::UInt32, 1)){
                return false;
            }
            else {
                out.enc2Lines = read_little_endian<std::uint32_t>(as_bytes(ar.state().parsing.itemHeader.payloadBytes));
            }

            // FTML::Archive::operator()(a2, "Sig");
            // FTML::SmartPointer::extract(...)
            // FTML::Archive::operator()(a2, "Enc1");
            // FTML::SmartPointer::extract(...)
            // FTML::Archive::operator()(a2, "Enc2");
            // FTML::SmartPointer::extract(...)
            // FTML::Archive::operator()(a2, "Clk");
            // FTML::SmartPointer::extract(...)
            // if (NumBM != 0) { FTML::Archive::operator()(a2, "BM"); FTML::SmartPointer::extract(...); }

            return true;
        }
    
        static bool GetXMSERawDataSet(Archive& ar, RawDataSet& out) {
            // DC::RawDataSet
            FTML::DC::GetDCRawDataSet(ar, out.dcRawDataSet);
            // auto const& archiveState = ar.state();
			if(!ar.get(TypeCode::ID::Int16, 1)){
                return false;
            }
            else {
                out.version = read_little_endian<float>(as_bytes(ar.state().parsing.itemHeader.payloadBytes));
            }
            // Get DCRecipe

			// Get DPRecipe

			// Get rotation
			if(!ar.get(TypeCode::ID::Bool, 1)){
                return false;
            }
            else {
				out.rotation.has = read_little_endian<std::uint16_t>(as_bytes(ar.state().parsing.itemHeader.payloadBytes));
                if (out.rotation.has) {
                    if (!ar.get(TypeCode::ID::Float, 1)) {
                        return false;
                    }
                    else {
                        out.rotation.value = read_little_endian<float>(as_bytes(ar.state().parsing.itemHeader.payloadBytes));
                    }
                }
            }
            
			// Get analyzerRef
            if(!ar.get(TypeCode::ID::Bool, 1)){
                return false;
            }
            else {
				out.analyzerRef.has = read_little_endian<std::uint16_t>(as_bytes(ar.state().parsing.itemHeader.payloadBytes));
                if(out.analyzerRef.has){
                    if(!ar.get(TypeCode::ID::Float, 1)){
                        return false;
                    }
                    else {
                        out.analyzerRef.value = read_little_endian<float>(as_bytes(ar.state().parsing.itemHeader.payloadBytes));
                    }
				}
            }

			// Get analyzerSample
            if (!ar.get(TypeCode::ID::Bool, 1)){
                return false;
            }
            else {
                out.analyzerSample.has = read_little_endian<float>(as_bytes(ar.state().parsing.itemHeader.payloadBytes));
                if (out.analyzerSample.has) {
                    if (!ar.get(TypeCode::ID::Float, 1)) {
                        return false;
                    }
                    else {
                        out.analyzerSample.value = read_little_endian<float>(as_bytes(ar.state().parsing.itemHeader.payloadBytes));
                    }
                }
            }

			// Get tilt

            // Get PixShiftRef
            if(!ar.get(TypeCode::ID::Bool, 1)){
                return false;
            }
            else {
                out.pixShiftRef.has = read_little_endian<std::uint16_t>(as_bytes(ar.state().parsing.itemHeader.payloadBytes));
                if(out.pixShiftRef.has){
                    if(!ar.get(TypeCode::ID::Float, 1)){
                        return false;
                    }
                    else {
                        out.pixShiftRef.value = read_little_endian<float>(as_bytes(ar.state().parsing.itemHeader.payloadBytes));
                    }
                }
			}

			// Get PixShiftSample
            if (!ar.get(TypeCode::ID::Bool, 1)){
                return false;
            }
            else {
                out.pixShiftSample.has = read_little_endian<std::uint16_t>(as_bytes(ar.state().parsing.itemHeader.payloadBytes));
                if (out.pixShiftSample.has) {
                    if (!ar.get(TypeCode::ID::Float, 1)) {
                        return false;
                    }
                    else {
                        out.pixShiftSample.value = read_little_endian<float>(as_bytes(ar.state().parsing.itemHeader.payloadBytes));
                    }
                }
			}
            
			// Get filterRef
			// Get filterSample
			// Get ref
            // dark
            
            /*
            * DCRecipe �����߼�
            */
            
            // sample
            // configSet

            return true;
		}
    }
}

namespace FTML::DC {
    // struct Configuration{

    // };

    // struct RawDataSet{
    //     std::string sampleID{};
    //     void* sysInfo{};
    // };

    namespace {
        bool GetDCConfiguration(Archive& ar, Configuration& out){
            return true;
        }

        bool GetDCRawDataSet(Archive& ar, RawDataSet& out){
            if(!ar.gets(out.sampleID,0x78)){
                return false;
            }

            // Get sysInfo
            return true;
        }
    }
}
