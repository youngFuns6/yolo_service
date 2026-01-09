import React, { lazy } from "react";
import { useRoutes, Navigate } from "react-router-dom";
import type { RouteObject } from "./types";
import lazyLoad from "@/routers/utils/lazyLoad";

const metaRouters = import.meta.glob("./modules/*.tsx", { eager: true });
export const routerArray: RouteObject[] = [];
Object.keys(metaRouters).forEach((item) => {
  Object.keys(metaRouters[item] as Object).forEach((key: any) => {
    routerArray.push(...(metaRouters[item] as any)[key]);
  });
});

export const rootRouter: RouteObject[] = [
  {
    path: "/",
    element: <Navigate to="/channel" replace />,
  },
  ...routerArray,
  {
    path: "/login",
    element: lazyLoad(lazy(() => import("@/pages/Login"))),
    meta: {
      noRequiresAuth: true
    }
  },
  //   {
  // 		path: "*",
  // 		element: <Navigate to="/404" />
  // 	}
]

const Router = () => {
  const rankRoutes = rootRouter.sort((a, b) => (a.meta?.rank || 0) - (b.meta?.rank || 0));
  return useRoutes(rankRoutes);
}

export default React.memo(Router);