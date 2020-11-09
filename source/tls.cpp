#include "tls.h"

namespace gin {
class TlsContext::Impl {
public:
};

TlsContext::TlsContext():impl{heap<TlsContext::Impl>()}{}
TlsContext::~TlsContext(){}
}