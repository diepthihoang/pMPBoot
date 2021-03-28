//
// placement.cpp
// Implementations of miscellaneous placement functions
// (e.g. for decoding -incremental parameters), that
// aren't wrapped up in classes but are, rather, put into
// the Placement namespace.
//
// Created by James Barbetti on 08-Oct-2020.
//

#include "placement.h"
#include <tree/phylotree.h>
#include <utils/stringfunctions.h> //for convert_int_nothrow
                                   //and convert_double_nothrow

PlacementParameters::PlacementParameters()
    : incremental_method(Params::getInstance().incremental_method){    
}

PlacementParameters::PlacementParameters(const char* incremental_method_text)
    : incremental_method(incremental_method_text) {
}

std::string PlacementParameters::getIncrementalParameter
            (const char letter, const char* defaultValue) const {
    const std::string& inc = incremental_method;
    std::string answer = defaultValue;
    int braceLevel = 0;
    int i;
    for (i=0; i<inc.length(); ++i) {
        if (inc[i]==letter && braceLevel==0 && (i==0 || inc[i-1]==',' || inc[i-1]=='+') ) {
            break;
        } else if (inc[i]=='{') {
            ++braceLevel;
        } else if (inc[i]=='}') {
            --braceLevel;
        }
    }
    if (i==inc.length()) {
        return answer;  //Didn't find it
    }
    ++i;
    int j;
    for (j=i; j<inc.length(); ++j) {
        if (inc[j]=='+' && braceLevel==0) {
            break;
        } else if (inc[j]=='-' && braceLevel==0) {
            break;
        } else if (inc[j]=='{') {
            ++braceLevel;
        } else if (inc[j]=='}') {
            --braceLevel;
        }
    }
    answer = inc.substr(i, j-i);
    if (!answer.empty() && answer[0]=='{'
        && answer[answer.length()-1]=='}' ) {
        answer = answer.substr(1, answer.length()-2);
    }
    return answer;
}

size_t  PlacementParameters::getIncrementalSizeParameter
        (const char letter, size_t defaultValue) const {
    auto s = getIncrementalParameter(letter, "");
    if (s.empty()) {
        return defaultValue;
    }
    int i = convert_int_nothrow(s.c_str(), static_cast<int>(defaultValue));
    if (i<0) {
        return defaultValue;
    }
    return static_cast<size_t>(i);
}
size_t  PlacementParameters::getNumberOfTaxaToRemoveAndReinsert
        (size_t countOfTaxa) const {
    if (countOfTaxa<4) {
        return 0;
    }
    string removalString = getIncrementalParameter('R', "");
    size_t len = removalString.length();
    if (len==0) {
        return 0;
    }
    size_t numberToRemove;
    if (removalString[len-1] == '%') {
        removalString = removalString.substr(0, len-1);
        double percent = convert_double_nothrow(removalString.c_str(), 0);
        if (percent<100.0/countOfTaxa) {
            return 0;
        } else if (100.0<=percent) {
            return 0; //Just ignore it. Todo: warn it's being ignored.
        }
        numberToRemove = (size_t) floor(percent * countOfTaxa / 100.0 + .5 );
    } else {
        numberToRemove = convert_int_nothrow(removalString.c_str(),0);
    }
    if (numberToRemove<1 || countOfTaxa <= numberToRemove+3) {
        return 0;
    }
    return numberToRemove;
}

bool  PlacementParameters::doesPlacementUseLikelihood() const {
    auto cf = getIncrementalParameter('C', "MP");
    return cf.find("ML") != std::string::npos;
}

bool  PlacementParameters::doesPlacementUseParsimony() const {
    auto cf = getIncrementalParameter('C', "MP");
    auto heuristic = getIncrementalParameter('H', "");
    return cf.find("MP") != std::string::npos
        || heuristic.find("MP") != std::string::npos;
}

bool  PlacementParameters::doesPlacementUseSankoffParsimony() const {
    auto cf = getIncrementalParameter('C', "MP");
    auto heuristic = getIncrementalParameter('H', "");

    return (cf == "SMP" || heuristic == "SMP");
}


PlacementParameters::LocalOptimization
PlacementParameters::getLocalOptimizationAlgorithm() const {
    auto f = getIncrementalParameter('L', "");
    return NO_LOCAL_OPTIMIZATION;
}

size_t  PlacementParameters::getTaxaPerBatch(size_t totalTaxa) const {
    size_t taxaPerBatch = getIncrementalSizeParameter('B', 0);
    if ( taxaPerBatch == 0 ) {
        taxaPerBatch = totalTaxa;
        if ( taxaPerBatch == 0 ) {
            taxaPerBatch = 1;
        }
    }
    return taxaPerBatch;
}

size_t  PlacementParameters::getInsertsPerBatch
        (size_t totalTaxa, size_t taxaPerBatch) const {
    string insertString = getIncrementalParameter('I', "");
    size_t len = insertString.length();
    if (len==0) {
        return taxaPerBatch;
    }
    size_t numberToInsert;
    if (insertString[len-1] == '%') {
        insertString = insertString.substr(0, len-1);
        double percent = convert_double_nothrow(insertString.c_str(), 0);
        if (percent<100.0/taxaPerBatch) {
            return 1;
        } else if (100.0<=percent) {
            return taxaPerBatch; //Just ignore it. Todo: warn it's being ignored.
        }
        numberToInsert = (size_t) floor(percent * taxaPerBatch / 100.0 + .5 );
    } else {
        numberToInsert = convert_int_nothrow(insertString.c_str(),0);
    }
    if (numberToInsert < 1 ) {
        numberToInsert = taxaPerBatch;
        if (numberToInsert < 1) {
            numberToInsert = 1;
        }
    }
    return numberToInsert;
}
PlacementParameters::BatchOptimization
PlacementParameters::getBatchOptimizationAlgorithm() const {
    auto f = getIncrementalParameter('A', "");
    return NO_BATCH_OPTIMIZATION;
}
PlacementParameters::GlobalOptimization
PlacementParameters::getGlobalOptimizationAlgorithm() const {
    auto f = getIncrementalParameter('T', "");
    return NO_GLOBAL_OPTIMIZATION;
}

