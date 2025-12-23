#pragma once

#include "catalog/catalog.hpp"

namespace velodb {

class TPCHCatalogBuilder {
public:
    static Catalog createTPCHCatalog();
};

} // namespace velodb
