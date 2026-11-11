#ifndef ADAPTER_IPTC_TEST_KERNEL_NETWORK_NAMESPACE_H
#define ADAPTER_IPTC_TEST_KERNEL_NETWORK_NAMESPACE_H

#include <string>

/* LEARN: moves the whole test process into a new, empty network namespace, so every rule it
   writes lives in tables nobody else sees and disappears when the process exits. It first tries
   a user namespace as well (no privilege needed where the kernel allows unprivileged user
   namespaces; the process becomes root inside it, with CAP_NET_ADMIN over the new network
   namespace), then a network namespace alone (real root with CAP_SYS_ADMIN). Must run before
   any thread starts: unshare(CLONE_NEWUSER) fails in a multi-threaded process.
   Returns an empty string on success, the reason otherwise.
*/
std::string enterIsolatedNetworkNamespace();

/* LEARN: the reason recorded by main(); empty means the tests run isolated. */
const std::string& isolationFailure();

#endif // ADAPTER_IPTC_TEST_KERNEL_NETWORK_NAMESPACE_H
