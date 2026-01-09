import type { RouteObject } from "../types";


export default (routes: RouteObject[]) => {
    return routes.sort((a, b) => (a.meta?.rank || 0) - (b.meta?.rank || 0));
}