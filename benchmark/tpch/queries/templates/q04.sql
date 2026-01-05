SELECT
    o_orderpriority
FROM
    orders JOIN lineitem ON o_orderkey = l_orderkey
WHERE
    o_orderdate >= DATE '1993-07-01'
    AND o_orderdate < DATE '1993-10-01'
    AND l_commitdate < l_receiptdate
ORDER BY
    o_orderpriority;