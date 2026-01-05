SELECT
    p_type,
    l_extendedprice,
    l_discount
FROM lineitem JOIN part ON l_partkey = p_partkey
WHERE
    l_shipdate >= DATE '1995-09-01'
    AND l_shipdate < DATE '1995-10-01';