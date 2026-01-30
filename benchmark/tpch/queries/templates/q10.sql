SELECT
    c_custkey,
    c_name,
    l_extendedprice,
    l_discount,
    c_acctbal,
    n_name,
    c_address,
    c_phone,
    c_comment
FROM
    nation,
    customer,
    orders,
    lineitem
WHERE
    c_nationkey = n_nationkey
    AND o_custkey = c_custkey
    AND l_orderkey = o_orderkey
    AND o_orderdate >= DATE '1993-10-01'
    AND o_orderdate < DATE '1994-01-01'
    AND l_returnflag = 'R'
LIMIT 20;
