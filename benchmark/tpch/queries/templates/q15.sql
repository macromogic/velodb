SELECT
    s_suppkey,
    s_name,
    s_address,
    s_phone,
    l_extendedprice,
    l_discount
FROM
    supplier JOIN lineitem ON s_suppkey = l_suppkey
ORDER BY
    s_suppkey;