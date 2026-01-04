SELECT
    l_shipmode,
    o_orderpriority
FROM
    orders JOIN lineitem ON o_orderkey = l_orderkey
WHERE
    (
        l_shipmode = 'MAIL' OR l_shipmode = 'SHIP'
    )
    AND l_commitdate < l_receiptdate
    AND l_shipdate < l_commitdate
    AND l_receiptdate >= '1994-01-01'
    AND l_receiptdate < '1995-01-01'
ORDER BY
    l_shipmode;
