import { lazy } from "react";
import { LayoutIndex } from "../constant";
import type { RouteObject } from "../types";
import lazyLoad from "../utils/lazyLoad";
import { FaCube } from "react-icons/fa";

export default [
  {
    path: "/model",
    name: "模型管理",
    icon: <FaCube />,
    element: <LayoutIndex />,
    meta: {
      rank: 1
    },
    children: [
      {
        index: true,
        name: "模型列表",
        element: lazyLoad(lazy(() => import("@/pages/Model/List"))),
      },
    ],
  },
] as RouteObject;

