SELECT
    l_returnflag,
    l_linestatus,
    l_quantity AS sum_qty,
    l_extendedprice AS sum_base_price,
    l_extendedprice * (1 - l_discount) AS sum_disc_price,
    l_extendedprice * (1 - l_discount) * (1 + l_tax) AS sum_charge,
    l_quantity AS avg_qty,
    l_extendedprice AS avg_price,
    l_discount AS avg_disc
FROM
    lineitem
WHERE
    l_shipdate <= '1998-09-02';
