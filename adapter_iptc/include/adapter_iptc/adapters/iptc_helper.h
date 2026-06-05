#ifndef LIBIPTC_HANDLE_H
#define LIBIPTC_HANDLE_H

#include <string>

struct ipt_entry;
struct xtc_handle;

class IptcHelper {
public:
  IptcHelper(const std::string &table, const std::string &chain);
  ~IptcHelper();

  void createChain();
  void flushEntries();
  void appendEntry(const ipt_entry *entry);
  void commit();

private:
  void ensureOpen();

  std::string m_table;
  std::string m_chain;
  xtc_handle *m_handle;
};

#endif /* LIBIPTC_HANDLE_H */
