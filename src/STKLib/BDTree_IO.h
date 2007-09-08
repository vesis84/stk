#ifndef BDTree_IO_h
#define BDTree_IO_h

namespace STK
{
  class BDTreeHeader;

  /** 
   * @brief BDTree IO header encapsulation
   */
  struct BDTreeHeader
  {
    static const char* INTRO ;

    int mHeaderSize;    ///< header size
    int mFileVersion;   ///< file version
    int mOrder;         ///< n-gram order
    int mVocabSize;     ///< vocabulary size
    int mBinary;        ///< true if binary, false if ASCII format
    int mExtra0;
    int mExtra1;


    /** 
     * @brief Reads header from open stream
     * @param rStream std::istream
     */
    void
    Read(std::istream& rStream);

    /** 
     * @brief Writes header to stream
     * @param rStream std::ostream
     */
    void
    Write(std::ostream rStream);
  }; //struct BDTreeHeader


} // namespace STK

#endif
