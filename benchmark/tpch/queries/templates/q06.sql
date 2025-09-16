-- TPC-H Query 6: Forecasting Revenue Change
-- This query quantifies the amount of revenue increase that would have resulted
-- from eliminating certain company-wide discounts in a given percentage range in a given year.

SELECT
    SUM(l_extendedprice * l_discount) as revenue
FROM
    lineitem
WHERE
    l_shipdate >= '1994-01-01'
    AND l_shipdate < '1995-01-01'
    AND l_discount BETWEEN 0.05 AND 0.07
    AND l_quantity < 24;
