/***************************************************************************
 *   copyright            : (C) 2004 by Lukas Burget,UPGM,FIT,VUT,Brno     *
 *   email                : burget@fit.vutbr.cz                            *
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "BDTree.h"
#include <cstring>

namespace STK
{
  const char* BDTreeHeader::INTRO = "#STKBTREE "; 


  //***************************************************************************/
  //***************************************************************************/
  //***************************************************************************/
  //***************************************************************************/
  void
  VecDistribution::
  Read(std::istream& rStream, BDTreeHeader& rHeader)
  {
    INT_32 aux_double;
    INT_32 i;
    INT_32 size;

    rStream.read(reinterpret_cast<char*>(&size), sizeof(size));
    assert(size == rHeader.mVocabSize);
    mVec.reserve(size);

    for (i = 0; i < size; ++i) {
      rStream.read(reinterpret_cast<char*>(&aux_double), sizeof(aux_double));
      mVec.push_back(aux_double);
    }
  }

  //***************************************************************************/
  //***************************************************************************/
  void
  VecDistribution::
  Write(std::ostream& rStream, BDTreeHeader& rHeader)
  {
    VecDistribution::Container::const_iterator i;
    DOUBLE_64 aux_double;
    INT_32    aux_int;

    aux_int = mVec.size();
    rStream.write(reinterpret_cast<char*>(&aux_int), sizeof(aux_int));

    for (i = mVec.begin(); i != mVec.end(); ++i) {
      aux_double = *i;
      rStream.write(reinterpret_cast<char*>(&aux_double), sizeof(aux_double));
    }
  }



  //***************************************************************************/
  //***************************************************************************/
  //***************************************************************************/
  //***************************************************************************/
  void
  BDTreeHeader::
  Write(std::ostream rStream)
  {
    char    aux_char;
    INT_32  aux_int;
    std::streampos stream_pos;

    stream_pos = rStream.tellp();

    rStream.write(BDTreeHeader::INTRO, 10);
    rStream.put(mBinary ? 'b' : 'a');

    mHeaderSize = rStream.tellp() - stream_pos + 6*sizeof(INT_32);

    if (mBinary) {
      aux_int = mHeaderSize;
      rStream.write(reinterpret_cast<char*>(&aux_int), sizeof(aux_int));
      
      aux_int = mFileVersion;
      rStream.write(reinterpret_cast<char*>(&aux_int), sizeof(aux_int));
      
      aux_int = mOrder;
      rStream.write(reinterpret_cast<char*>(&aux_int), sizeof(aux_int));
      
      aux_int = mVocabSize;
      rStream.write(reinterpret_cast<char*>(&aux_int), sizeof(aux_int));

      aux_int = mExtra0;
      rStream.write(reinterpret_cast<char*>(&aux_int), sizeof(aux_int));

      aux_int = mExtra1;
      rStream.write(reinterpret_cast<char*>(&aux_int), sizeof(aux_int));
    }
    else {
      throw runtime_error("ASCII format not supported yet");
    }
  }

  //***************************************************************************/
  //***************************************************************************/
  void
  BDTreeHeader::
  Read(std::istream& rStream)
  {
    char    aux_char;
    INT_32  aux_int
    char    p_intro[strlen(BDTreeHeader::INTRO)];

    // read intro and check it
    rStream.read(p_intro, strlen(BDTreeHeader::INTRO));
    if (strcmp(p_intro, BDTreeHeader::INTRO)) {
    }
    
    // binary or ascii???
    rStream.get(aux_char);
    switch(aux_char) {
      case 'a': mBinary = false; break;
      case 'b': mBinary = true; break;
      default : throw runtime_error("Format not supported");
    }

    if (mBinary) {
      rStream.read(reinterpret_cast<char*>(&aux_int), sizeof(aux_int));
      mHeaderSize  = aux_int;

      rStream.read(reinterpret_cast<char*>(&aux_int), sizeof(aux_int));
      mFileVersion = aux_int;

      rStream.read(reinterpret_cast<char*>(&aux_int), sizeof(aux_int));
      mOrder       = aux_int;

      rStream.read(reinterpret_cast<char*>(&aux_int), sizeof(aux_int));
      mVocabSize   = aux_int;

      rStream.read(reinterpret_cast<char*>(&aux_int), sizeof(aux_int));
      mExtra0      = aux_int;

      rStream.read(reinterpret_cast<char*>(&aux_int), sizeof(aux_int));
      mExtra1      = aux_int;
    }
    else { // if (Binary) 
      // TODO: Implement this
      throw runtime_error("ASCII format not supported yet");
    }
  }


  //***************************************************************************/
  //***************************************************************************/
  //***************************************************************************/
  //***************************************************************************/
  void
  BDTree::
  Read(std::istream& rStream, BDTreeHeader& rHeader)
  {
    INT_32 flag = 0;

    // read main flag
    rStream.read(reinterpret_cast<char*>(&flag), sizeof(flag));
    
    if (flag & 1) {
      mpQuestion  = new BSetQuestion(rStream, rHeader);
      mpTree0     = new BDTree(rStream, rHeader);
      mpTree1     = new BDTree(rStream, rHeader);
    }
    else {
      mDist.Read(rStream, rHeader);
    }
  } // Read(std::istream& rStream, BDTreeHeader& rHeader)


  //***************************************************************************/
  //***************************************************************************/
  void
  BDTree::
  Write(std::ostream& rStream, BDTreeHeader& rHeader)
  {
    INT_32 flag = 0;

    if (!IsLeaf()) {
      flag &= 1;
    }

    // write main flag
    rStream.write(reinterpret_cast<char*>(&flag), sizeof(flag));

    if (IsLeaf()) {
      mDist.Write(rStream, rHeader);
    }
    else {
      mpQuestion->Write(rStream, rHeader);
      mpTree0->Write(rStream, rHeader);
      mpTree1->Write(rStream, rHeader);
    }
  } // Write(std::ostream& rStream, BDTreeHeader& rHeader)


  //***************************************************************************/
  //***************************************************************************/
  //***************************************************************************/
  //***************************************************************************/
  void
  BSetQuestion::
  Read(std::istream& rStream, BDTreeHeader& rHeader)
  {
    INT_32 aux_int;
    INT_32 i;
    INT_32 size;

    rStream.read(reinterpret_cast<char*>(&size), sizeof(size));
    assert(size == rHeader.mVocabSize);
    mSet.reserve(size);

    for (i = 0; i < size; ++i) {
      rStream.read(reinterpret_cast<char*>(&aux_int), sizeof(aux_int));
      mSet.push_back(aux_int);
    }
  }


  //***************************************************************************/
  //***************************************************************************/
  void
  BSetQuestion::
  Write(std::ostream& rStream, BDTreeHeader& rHeader)
  {
    BSetQuestion::SetType::const_iterator i;
    INT_32 aux_int;

    aux_int = mSet.size();
    rStream.write(reinterpret_cast<char*>(&aux_int), sizeof(aux_int));

    for (i = mSet.begin(); i != mSet.end(); ++i) {
      aux_int = *i ? 1 : 0;
      rStream.write(reinterpret_cast<char*>(&aux_int), sizeof(aux_int));
    }
  }
} //namespace STK