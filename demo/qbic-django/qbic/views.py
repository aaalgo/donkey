from thrift import Thrift
from thrift.transport import TSocket
from thrift.transport import TTransport
from thrift.protocol import TBinaryProtocol
from donkey import Donkey
from donkey.ttypes import *
from django.core.context_processors import csrf
from django.shortcuts import redirect, render_to_response

def search (request):
    url = ''
    content = ''
    if 'file' in request.FILES:
        content = request.FILES['file'].read()
    if 'url' in request.REQUEST:
        url = request.REQUEST['url']
    hits = []
    try:
        transport = TSocket.TSocket('localhost', 50052)
        transport = TTransport.TBufferedTransport(transport)
        protocol = TBinaryProtocol.TBinaryProtocol(transport)
        client = Donkey.Client(protocol)
        transport.open()
        request = SearchRequest(db=0, raw=True, url=url, content=content, type='')
        resp = client.search(request)
        hits = resp.hits
        transport.close()
    except:
        pass
    c = {'hits': hits}
    c.update(csrf(request))
    return render_to_response("qbic/search.html", c)


