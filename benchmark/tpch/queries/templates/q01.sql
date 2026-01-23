SELECT
    l_returnflag,
    l_linestatus,
    l_quantity,
    l_extendedprice,
    l_discount,
    l_tax
FROM
    lineitem
WHERE
    l_shipdate <= '1998-09-02';
