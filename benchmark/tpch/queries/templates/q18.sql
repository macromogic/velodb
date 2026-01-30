SELECT
    c_name,
    c_custkey,
    o_orderkey,
    o_orderdate,
    o_totalprice,
    l_quantity
FROM
    customer,
    orders,
    lineitem
WHERE
    o_custkey = c_custkey
    AND l_orderkey = o_orderkey
ORDER BY
    o_totalprice DESC,
    o_orderdate
LIMIT 100;
