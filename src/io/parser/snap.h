/****************************************************************************
**
** Copyright (C) 2013 EPFL-LTS2
** Contact: Kirell Benzi (first.last@epfl.ch)
**
** This file is part of Graphilt.
**
**
** GNU General Public License Usage
** This file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPLv3 included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements
** will be met: http://www.gnu.org/licenses/
**
****************************************************************************/

#ifndef GHT_SNAP_H
#define GHT_SNAP_H

#include <iostream>
#include <fstream>
#include <string>

#include <viennacl/meta/result_of.hpp>
#include <viennacl/traits/size.hpp>

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

#include "util/log.h"

static const uint32_t kMax = std::numeric_limits<uint32_t>::max();
static const int kDefaultWeight = 1;

namespace ba = boost::algorithm;

namespace boost {
template<>
inline uint32_t lexical_cast( const std::string& arg )
{
    char* stop;
    uint32_t res = strtol( arg.c_str(), &stop, 10 );
    if( *stop != 0 )
        throw_exception(bad_lexical_cast(typeid(uint32_t), typeid(std::string)));
    return res;
}
} //namespace boost

namespace ght {
namespace io {

namespace detail {

template<class Matrix, class Scalar>
void appendInMatrix( Matrix& matrix, uint32_t row, uint32_t col, Scalar val )
{
    matrix(row, col) += val;
}

template<class Scalar>
void appendInMatrix( std::vector< std::map<uint32_t, Scalar> >& matrix, uint32_t row, uint32_t col, Scalar val )
{
    // Default constructed value is 0
    matrix[row][col] += val;
}

template<class Matrix>
void resizeMatrix( Matrix& matrix, uint32_t row )
{
    matrix = Matrix(row, row);
}

template<class Scalar>
void resizeMatrix( std::vector<std::map<uint32_t, Scalar> >& matrix, uint32_t row )
{
    matrix.resize(row);
}


} // end namespace detail

//bool parseSnapFormat

template<class Matrix>
bool parseSnapFormat( const std::string& filename, Matrix& matrix )
{
//    typedef typename viennacl::result_of::value_type<MatrixType>::type ScalarType;

    LOG(logINFO) << "Parsing SNAP undirected graph: " << filename;

    std::ifstream file(filename.c_str());

    if( !file ) {
        LOG(logERROR) << "parseSnapFormat: cannot open file " << filename;
        return false;
    }

    std::string read_line;
    uint32_t nodeCount = 0;
    uint32_t edgeCount = 0;
    while( std::getline(file, read_line) ) {
        if( !ba::starts_with(read_line, "#") ) {
            break;
        }
        else {
            if( ba::starts_with(read_line, "# Nodes:") ) {
                std::vector<std::string> tokens;
                ba::split(tokens, read_line, boost::is_any_of(" "));
                // Get numbers of nodes
                try {
                    nodeCount = boost::lexical_cast<uint32_t>(tokens.at(2));
                    edgeCount = boost::lexical_cast<uint32_t>(tokens.at(4));
                }
                catch( boost::bad_lexical_cast& ) {
                    LOG(logERROR) << "parseSnapFormat invalid cast " << tokens.at(2);
                    return false;
                }
            }
        }
    }

    if( nodeCount == 0 ) {
        LOG(logERROR) << "parseSnapFormat invalid nodeCount";
        return false;
    }
    if( edgeCount == 0 ) {
        LOG(logERROR) << "parseSnapFormat invalid edgeCount";
        return false;
    }

    try {
        // Set Laplacian matrix size, default constructed values should be set to 0
        detail::resizeMatrix(matrix, nodeCount);
    } catch( std::bad_alloc& ) {
        LOG(logERROR) << "parseSnapFormat: caught bad alloc exception, dataset too large for RAM";
        return false;
    } catch( const std::exception& e ) {
        LOG(logERROR) << "parseSnapFormat: " << e.what();
        return false;
    } catch(...) {
        LOG(logERROR) << "parseSnapFormat: unknown exception";
        return false;
    }

//    // Go backwards one line
    long pos = file.tellg();
    long startPos = pos - (read_line.size() + 1);
    file.seekg(startPos);

    // Fill Map for Ids to matrix rows
    std::map<uint32_t, uint32_t> indexMap;
    // Actual parsing starts here
    uint32_t idx = 0;
    for( uint32_t i = 0; i < edgeCount && !file.eof(); ++i ) {
        uint32_t startId = kMax;
        uint32_t endId = kMax;
        file >> startId >> endId;
        if( startId == endId )
            continue;
        if( startId == kMax || endId == kMax ) {
            LOG(logWARNING) << "Startid or EndId == kMax";
            break;
        }

        uint32_t row = 0;
        uint32_t col = 0;

        auto it = indexMap.find(startId);
        // Not in map
        if( it == indexMap.end() ) {
            indexMap[startId] = idx;
            row = idx;
            idx++;
        }
        else {
            row = it->second;
        }

        it = indexMap.find(endId);
        if( it == indexMap.end() ) {
            indexMap[endId] = idx;
            col = idx;
            idx++;
        }
        else {
            col = it->second;
        }
        // Add on diagonal
        detail::appendInMatrix(matrix, row, row, kDefaultWeight);
        // Add edge weight
        detail::appendInMatrix(matrix, row, col, -kDefaultWeight);
    }

    file.close();

    return true;
}

} // end namespace io
} // end namespace ght

#endif // GHT_SNAP_H
