/***************************************************************************
 *   Copyright (C) 2006 by BUI Quang Minh, Steffen Klaere, Arndt von Haeseler   *
 *   minh.bui@univie.ac.at   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include <iostream>
#include <fstream>
//#include "node.h"
#include "ncl/ncl.h"
#include "utils/gzstream.h"

/**
	MyReader class to make more informative message
*/
class MyReader : public NxsReader
{
public:
	
	/**
		input stream
	*/
    pigzstream inf;
    
	/**
		constructor
		@param infname input file name
	*/
	MyReader(const char *infname) : NxsReader(), inf("nexus")
	{
		inf.open(infname, ios::binary | ios::in);
        if (!inf.is_open()) {
			outError(ERR_READ_INPUT);
        }
	}

	/**
		destructor
	*/
	virtual ~MyReader()
	{
		inf.close();
	}

	/**
		start
	*/
	virtual void ExecuteStarting() {}
	/**
		stop
	*/
	virtual void ExecuteStopping() {}

    /**
     enter a block
     @param blockName block name
     @return true always
     */
    virtual bool EnteringBlock(NxsString blockName)
    {
        if (verbose_mode >= VB_MED) {
            inf.hideProgress();
            cout << "Reading \"" << blockName << "\" block..." << endl;
            inf.showProgress();
        }
        
        // Returning true means it is ok to delete any data associated with
        // blocks of this type read in previously
        //
        return true;
    }

    /**
     skip a block
     @param blockName block name
     */
    virtual void SkippingBlock(NxsString blockName)
    {
        inf.hideProgress();
        cout << "Skipping unknown block (" << blockName << ")..." << endl;
        inf.showProgress();
    }

	//virtual void SkippingDisabledBlock(NxsString blockName) {}

	/**
		print comments
		@param comment comment string
	*/
	virtual void	OutputComment(const NxsString &comment)
	{
		//cout << comment;
	}

	/**
		called when error occurs
		@param msg additional message
		@param pos file position
		@param line line number
		@param col column number
	*/
	virtual void	NexusError(NxsString msg, file_pos pos, long line, long col)
	{
        inf.hideProgress();
		cerr << endl;
		cerr << "Error found at line " << line;
		cerr << ", column " << col;
		cerr << " (file position " << pos << "):" << endl;
		cerr << msg << endl;
		exit(1);
	}
};

/**
	MyToken class to make more informative message
*/
class MyToken : public NxsToken
{
public:

	/**
		constructor
		@param is input stream
	*/
	MyToken(istream &is) : NxsToken(is) {}

	/**
		print comments
		@param msg comment string
	*/
	virtual void OutputComment(const NxsString &msg)
	{
		//cout << msg << endl;
	}


};
