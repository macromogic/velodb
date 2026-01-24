SELECT
    l_extendedprice,
    l_discount
FROM lineitem JOIN part ON l_partkey = p_partkey
WHERE (
    p_brand = 'Brand#12'
    AND p_container IN ('SM CASE', 'SM BOX', 'SM PACK', 'SM PKG')
    AND l_quantity >= 1.0 AND l_quantity <= 11.0
    AND p_size BETWEEN 1 AND 5
    AND l_shipmode IN ('AIR', 'AIR REG')
    AND l_shipinstruct = 'DELIVER IN PERSON'
  );
