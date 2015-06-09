from django.conf.urls import patterns, include, url

#from django.contrib import admin
#admin.autodiscover()

from views import *

urlpatterns = patterns('',
    # Examples:
    # url(r'^$', 'qbic.views.home', name='home'),
    # url(r'^blog/', include('blog.urls')),
    url(r'^search/$', 'qbic.views.search', name='qbic_search'),
)

